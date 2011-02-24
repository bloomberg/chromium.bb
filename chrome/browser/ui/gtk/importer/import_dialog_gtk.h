// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_DIALOG_GTK_H_
#pragma once

#include "chrome/browser/importer/importer.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;

class ImportDialogGtk : public ImportObserver,
                        public ImporterList::Observer {
 public:
  // Displays the import box to import data from another browser into |profile|
  // |initial_state| is a bitmask of ImportItems. Each checkbox for the bits in
  // is checked.
  static void Show(GtkWindow* parent, Profile* profile, int initial_state);

  // ImportObserver implementation.
  virtual void ImportCanceled();
  virtual void ImportComplete();

 private:
  ImportDialogGtk(GtkWindow* parent, Profile* profile, int initial_state);
  ~ImportDialogGtk();

  // ImporterList::Observer implementation.
  virtual void SourceProfilesLoaded();

  // Handler to respond to OK or Cancel responses from the dialog.
  CHROMEGTK_CALLBACK_1(ImportDialogGtk, void, OnDialogResponse, int);

  // Handler to respond to widget clicked actions from the dialog.
  CHROMEGTK_CALLBACK_0(ImportDialogGtk, void, OnDialogWidgetClicked);

  // Enable or disable the dialog buttons depending on the state of the
  // checkboxes.
  void UpdateDialogButtons();

  // Sets the sensitivity of all controls on the dialog except the cancel
  // button.
  void SetDialogControlsSensitive(bool sensitive);

  // Create a bitmask from the checkboxes of the dialog.
  uint16 GetCheckedItems();

  // Parent window
  GtkWindow* parent_;

  // Import Dialog
  GtkWidget* dialog_;

  // Combo box that displays list of profiles from which we can import.
  GtkWidget* combo_;

  // Bookmarks/Favorites checkbox
  GtkWidget* bookmarks_;

  // Search Engines checkbox
  GtkWidget* search_engines_;

  // Passwords checkbox
  GtkWidget* passwords_;

  // History checkbox
  GtkWidget* history_;

  // Import button.
  GtkWidget* import_button_;

  // Our current profile
  Profile* profile_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  // Enumerates the source profiles.
  scoped_refptr<ImporterList> importer_list_;

  int initial_state_;

  DISALLOW_COPY_AND_ASSIGN(ImportDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_DIALOG_GTK_H_
