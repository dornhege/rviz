/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>

#include "rviz/display_factory.h"
#include "rviz/display.h"
#include "rviz/display_group.h"
#include "rviz/add_display_dialog.h"
#include "rviz/properties/property.h"
#include "rviz/properties/property_tree_widget.h"
#include "rviz/properties/property_tree_with_help.h"
#include "rviz/visualization_manager.h"
#include "rviz/yaml_config_reader.h"
#include "rviz/yaml_config_writer.h"

#include "rviz/displays_panel.h"

namespace fs = boost::filesystem;

// Copy from visualization_frame.cpp, where these were hardcoded
#define CONFIG_EXTENSION "rviz"
#define CONFIG_EXTENSION_WILDCARD "*." CONFIG_EXTENSION

namespace rviz
{

DisplaysPanel::DisplaysPanel( QWidget* parent )
  : Panel( parent )
{
  tree_with_help_ = new PropertyTreeWithHelp;
  property_grid_ = tree_with_help_->getTree();

  QPushButton* add_button = new QPushButton( "Add" );
  add_button->setShortcut( QKeySequence( QString( "Ctrl+N" )));
  add_button->setToolTip( "Add a new display, Ctrl+N" );
  duplicate_button_ = new QPushButton( "Duplicate" );
  duplicate_button_->setShortcut( QKeySequence( QString( "Ctrl+D" )));
  duplicate_button_->setToolTip( "Duplicate a display, Ctrl+D" );
  duplicate_button_->setEnabled( false );
  remove_button_ = new QPushButton( "Remove" );
  remove_button_->setShortcut( QKeySequence( QString( "Ctrl+X" )));
  remove_button_->setToolTip( "Remove displays, Ctrl+X" );
  remove_button_->setEnabled( false );
  rename_button_ = new QPushButton( "Rename" );
  rename_button_->setShortcut( QKeySequence( QString( "Ctrl+R" )));
  rename_button_->setToolTip( "Rename a display, Ctrl+R" );
  rename_button_->setEnabled( false );

  load_group_button_ = new QPushButton( "Load Group" );
  load_group_button_->setToolTip( "Load a group display" );
  load_group_button_->setEnabled( true );
  save_group_button_ = new QPushButton( "Save Group" );
  save_group_button_->setToolTip( "Save a group display" );
  save_group_button_->setEnabled( false );

  QHBoxLayout* button_layout = new QHBoxLayout;
  button_layout->addWidget( add_button );
  button_layout->addWidget( duplicate_button_ );
  button_layout->addWidget( remove_button_ );
  button_layout->addWidget( rename_button_ );
  button_layout->setContentsMargins( 2, 0, 2, 2 );

  QHBoxLayout* save_button_layout = new QHBoxLayout;
  save_button_layout->addWidget( load_group_button_ );
  save_button_layout->addWidget( save_group_button_ );
  save_button_layout->setContentsMargins( 2, 0, 2, 2 );

  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins( 0, 0, 0, 2 );
  layout->addWidget( tree_with_help_ );
  layout->addLayout( button_layout );
  layout->addLayout( save_button_layout );

  setLayout( layout );

  connect( add_button, SIGNAL( clicked( bool )), this, SLOT( onNewDisplay() ));
  connect( duplicate_button_, SIGNAL( clicked( bool )), this, SLOT( onDuplicateDisplay() ));
  connect( remove_button_, SIGNAL( clicked( bool )), this, SLOT( onDeleteDisplay() ));
  connect( rename_button_, SIGNAL( clicked( bool )), this, SLOT( onRenameDisplay() ));
  connect( load_group_button_, SIGNAL( clicked( bool )), this, SLOT( onLoadGroupDisplay() ));
  connect( save_group_button_, SIGNAL( clicked( bool )), this, SLOT( onSaveGroupDisplay() ));
  connect( property_grid_, SIGNAL( selectionHasChanged() ), this, SLOT( onSelectionChanged() ));
}

DisplaysPanel::~DisplaysPanel()
{
}

void DisplaysPanel::onInitialize()
{
  property_grid_->setModel( vis_manager_->getDisplayTreeModel() );
}

void DisplaysPanel::onNewDisplay()
{
  QString lookup_name;
  QString display_name;
  QString topic;
  QString datatype;

  QStringList empty;

  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  AddDisplayDialog* dialog = new AddDisplayDialog( vis_manager_->getDisplayFactory(),
                                                   "Display",
                                                   empty, empty,
                                                   &lookup_name,
                                                   &display_name,
                                                   &topic,
                                                   &datatype );
  QApplication::restoreOverrideCursor();

  vis_manager_->stopUpdate();
  if( dialog->exec() == QDialog::Accepted )
  {
    Display *disp = vis_manager_->createDisplay( lookup_name, display_name, true );
    if ( !topic.isEmpty() && !datatype.isEmpty() )
    {
      disp->setTopic( topic, datatype );
    }
  }
  vis_manager_->startUpdate();
  activateWindow(); // Force keyboard focus back on main window.
  delete dialog;
}

void DisplaysPanel::onDuplicateDisplay()
{
  QList<Display*> displays_to_duplicate = property_grid_->getSelectedObjects<Display>();

  QList<Display*> duplicated_displays;

  for( int i = 0; i < displays_to_duplicate.size(); i++ )
  {
    // initialize display
    QString lookup_name = displays_to_duplicate[ i ]->getClassId();
    QString display_name = displays_to_duplicate[ i ]->getName();
    Display *disp = vis_manager_->createDisplay( lookup_name, display_name, true );
    // duplicate config
    Config config;
    displays_to_duplicate[ i ]->save(config);
    disp->load(config);
    duplicated_displays.push_back(disp);
  }
  // make sure the newly duplicated displays are selected.
  if (duplicated_displays.size() > 0) {
    QModelIndex first = property_grid_->getModel()->indexOf(duplicated_displays.front());
    QModelIndex last = property_grid_->getModel()->indexOf(duplicated_displays.back());
    QItemSelection selection(first, last);
    property_grid_->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
  }
  vis_manager_->startUpdate();
  activateWindow(); // Force keyboard focus back on main window.
}

void DisplaysPanel::onDeleteDisplay()
{
  QList<Display*> displays_to_delete = property_grid_->getSelectedObjects<Display>();

  QModelIndex new_selected;

  for( int i = 0; i < displays_to_delete.size(); i++ )
  {
    if (i == 0) {
      QModelIndex first = property_grid_->getModel()->indexOf(displays_to_delete[i]);
      // This is safe because the first few rows cannot be deleted (they aren't "displays").
      new_selected = first.sibling(first.row() - 1, first.column());
    }
    // Displays can emit signals from other threads with self pointers.  We're
    // freeing the display now, so ensure no one is listening to those signals.
    displays_to_delete[ i ]->disconnect();
    // Delete display later in case there are pending signals to it.
    displays_to_delete[ i ]->deleteLater();
  }

  QItemSelection selection(new_selected, new_selected);
  property_grid_->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);

  vis_manager_->notifyConfigChanged();
}

void DisplaysPanel::onSelectionChanged()
{
  QList<Display*> displays = property_grid_->getSelectedObjects<Display>();

  int num_displays_selected = displays.size();
  bool is_group = num_displays_selected == 1 &&
      (dynamic_cast<DisplayGroup*>(property_grid_->getSelectedObjects<Display>()[0]) != NULL);

  duplicate_button_->setEnabled( num_displays_selected > 0 );
  remove_button_->setEnabled( num_displays_selected > 0 );
  rename_button_->setEnabled( num_displays_selected == 1 );
  save_group_button_->setEnabled( is_group );
}

void DisplaysPanel::onRenameDisplay()
{
  QList<Display*> displays = property_grid_->getSelectedObjects<Display>();
  if( displays.size() == 0 )
  {
    return;
  }
  Display* display_to_rename = displays[ 0 ];

  if( !display_to_rename )
  {
    return;
  }

  QString old_name = display_to_rename->getName();
  QString new_name = QInputDialog::getText( this, "Rename Display", "New Name?", QLineEdit::Normal, old_name );

  if( new_name.isEmpty() || new_name == old_name )
  {
    return;
  }

  display_to_rename->setName( new_name );
}

void DisplaysPanel::onSaveGroupDisplay()
{
  QList<Display*> displays = property_grid_->getSelectedObjects<Display>();
  if( displays.size() != 1 )
  {
    return;
  }
  Display* display_to_save = displays[ 0 ];
  if( !display_to_save )
  {
    return;
  }

  vis_manager_->stopUpdate();
  QString save_filename = QFileDialog::getSaveFileName( this, "Choose a file to save to",
                                                     QString(),
                                                     "RViz config files (" CONFIG_EXTENSION_WILDCARD ")" );
  vis_manager_->startUpdate();

  if(save_filename.isEmpty())
      return;

  std::string filename = save_filename.toStdString();
  fs::path path( filename );
  if( path.extension() != "." CONFIG_EXTENSION )
  {
      filename += "." CONFIG_EXTENSION;
  }

  Config config;
  display_to_save->save( config );

  YamlConfigWriter writer;
  writer.writeFile( config, QString::fromStdString(filename) );

  QString error_message;
  bool ok = true;
  if( writer.error() )
  {
    ROS_ERROR( "%s", qPrintable( writer.errorMessage() ));
    error_message = writer.errorMessage();
    ok = false;
  } else {
    error_message = "";
    ok = true;
  }

  if(!ok) {
      QMessageBox::critical(this, "Failed to save.", error_message);
  }
}

void DisplaysPanel::onLoadGroupDisplay()
{
  vis_manager_->stopUpdate();
  QString filename = QFileDialog::getOpenFileName( this, "Choose a file to open",
          QString(),
          "RViz config files (" CONFIG_EXTENSION_WILDCARD ")"  );
  vis_manager_->startUpdate();

  if(filename.isEmpty())
      return;

  std::string path = filename.toStdString();
  if( !fs::exists( path ))
  {
      QString message = filename + " does not exist!";
      QMessageBox::critical( this, "Config file does not exist", message );
      return;
  }

  YamlConfigReader reader;
  Config config;
  reader.readFile(config, filename);
  if(reader.error())
      return;

  vis_manager_->loadGroup(config);
}

void DisplaysPanel::save( Config config ) const
{
  Panel::save( config );
  tree_with_help_->save( config );
}

void DisplaysPanel::load( const Config& config )
{
  Panel::load( config );
  tree_with_help_->load( config );
}

} // namespace rviz
