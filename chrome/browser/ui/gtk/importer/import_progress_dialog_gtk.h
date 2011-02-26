// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_PROGRESS_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_PROGRESS_DIALOG_GTK_H_
#pragma once

#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWindow GtkWindow;
typedef struct _GtkWidget Widget;

class Profile;

class ImportProgressDialogGtk : public ImporterHost::Observer {
 public:
  // Displays the import progress dialog box and starts the import.
  static void StartImport(GtkWindow* parent,
                          int16 items,
                          ImporterHost* importer_host,
                          const ProfileInfo& browser_profile,
                          Profile* profile,
                          ImportObserver* observer,
                          bool first_run);

  // Overridden from ImporterHost::Observer:
  virtual void ImportItemStarted(importer::ImportItem item);
  virtual void ImportItemEnded(importer::ImportItem item);
  virtual void ImportStarted();
  virtual void ImportEnded();

 private:
  ImportProgressDialogGtk(const string16& source_profile,
                          int16 items,
                          ImporterHost* importer_host,
                          ImportObserver* observer,
                          GtkWindow* parent,
                          bool bookmarks_import);
  virtual ~ImportProgressDialogGtk();

  CHROMEGTK_CALLBACK_1(ImportProgressDialogGtk, void, OnResponse, int);

  void ShowDialog();

  void CloseDialog();

  // Parent window
  GtkWindow* parent_;

  // Import progress dialog
  GtkWidget* dialog_;

  // Bookmarks/Favorites checkbox
  GtkWidget* bookmarks_;

  // Search Engines checkbox
  GtkWidget* search_engines_;

  // Passwords checkbox
  GtkWidget* passwords_;

  // History checkbox
  GtkWidget* history_;

  // Boolean that tells whether we are currently in the mid of import process
  bool importing_;

  // Observer that we need to notify about import events
  ImportObserver* observer_;

  // Items to import from the other browser.
  int16 items_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  DISALLOW_COPY_AND_ASSIGN(ImportProgressDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_PROGRESS_DIALOG_GTK_H_
