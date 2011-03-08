// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_PROGRESS_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_PROGRESS_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "ui/base/gtk/gtk_signal.h"

class ImporterHost;
class ImporterObserver;
class Profile;

typedef struct _GtkWidget Widget;
typedef struct _GtkWindow GtkWindow;

class ImportProgressDialogGtk : public importer::ImporterProgressObserver {
 public:
  // Displays the import progress dialog box and starts the import.
  static void StartImport(GtkWindow* parent,
                          uint16 items,
                          ImporterHost* importer_host,
                          ImporterObserver* importer_observer,
                          const importer::ProfileInfo& browser_profile,
                          Profile* profile,
                          bool first_run);

 private:
  ImportProgressDialogGtk(GtkWindow* parent,
                          uint16 items,
                          ImporterHost* importer_host,
                          ImporterObserver* importer_observer,
                          const string16& source_profile,
                          bool bookmarks_import);
  virtual ~ImportProgressDialogGtk();

  CHROMEGTK_CALLBACK_1(ImportProgressDialogGtk, void, OnResponse, int);

  void ShowDialog();

  void CloseDialog();

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE;
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void ImportEnded() OVERRIDE;

  // Parent window.
  GtkWindow* parent_;

  // Import progress dialog.
  GtkWidget* dialog_;

  // Bookmarks/Favorites checkbox.
  GtkWidget* bookmarks_;

  // Search Engines checkbox.
  GtkWidget* search_engines_;

  // Passwords checkbox.
  GtkWidget* passwords_;

  // History checkbox.
  GtkWidget* history_;

  // Items to import from the other browser.
  uint16 items_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  // Observer that we need to notify about import events.
  ImporterObserver* importer_observer_;

  // True if the import operation is in progress.
  bool importing_;

  DISALLOW_COPY_AND_ASSIGN(ImportProgressDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_PROGRESS_DIALOG_GTK_H_
