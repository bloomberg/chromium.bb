// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_LOCK_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_LOCK_DIALOG_GTK_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

// ImportLockDialogGtk presents the dialog which asks the user to shut down
// Firefox before starting the profile import.
class ImportLockDialogGtk {
 public:
  static void Show(GtkWindow* parent,
                   const base::Callback<void(bool)>& callback);

 private:
  ImportLockDialogGtk(GtkWindow* parent,
                      const base::Callback<void(bool)>& callback);
  ~ImportLockDialogGtk();

  CHROMEGTK_CALLBACK_1(ImportLockDialogGtk, void, OnResponse, int);

  // Import lock dialog.
  GtkWidget* dialog_;

  // Called with the result of the dialog.
  base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(ImportLockDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_IMPORTER_IMPORT_LOCK_DIALOG_GTK_H_
