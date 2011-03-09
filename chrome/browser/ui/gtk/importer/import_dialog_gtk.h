// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_observer.h"
#include "ui/base/gtk/gtk_signal.h"

class ImporterHost;
class Profile;

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

// ImportDialogGtk presents the dialog that allows the user to select what to
// import from other browsers.
class ImportDialogGtk : public ImporterList::Observer,
                        public ImporterObserver {
 public:
  // Displays the import box to import data from another browser into |profile|.
  // |initial_state| is a bitmask of importer::ImportItem.
  // Each checkbox for the bits in |initial_state| is checked.
  static void Show(GtkWindow* parent, Profile* profile, uint16 initial_state);

 private:
  ImportDialogGtk(GtkWindow* parent, Profile* profile, uint16 initial_state);
  virtual ~ImportDialogGtk();

  // Handler to respond to OK or Cancel responses from the dialog.
  CHROMEGTK_CALLBACK_1(ImportDialogGtk, void, OnResponse, int);

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

  // ImporterList::Observer:
  virtual void SourceProfilesLoaded() OVERRIDE;

  // ImporterObserver:
  virtual void ImportCompleted() OVERRIDE;
  virtual void ImportCanceled() OVERRIDE;

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

  // Initial state of the |checkbox_items_|.
  uint16 initial_state_;

  DISALLOW_COPY_AND_ASSIGN(ImportDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_DIALOG_GTK_H_
