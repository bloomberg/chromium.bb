// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
#define CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/file_path.h"

class Browser;

typedef struct _GtkPrintJob GtkPrintJob;

// Currently this dialog only allows the user to choose a printer.
class PrintDialogGtk {
 public:
  static bool DialogShowing();

  // Called on the IO thread.
  static void CreatePrintDialogForPdf(const FilePath& path);

 private:
  explicit PrintDialogGtk(const FilePath& path_to_pdf);
  ~PrintDialogGtk();

  static void CreateDialogImpl(const FilePath& path);

  static void OnResponseThunk(GtkDialog* dialog,
                              gint response_id,
                              gpointer user_data) {
    reinterpret_cast<PrintDialogGtk*>(user_data)->OnResponse(response_id);
  }
  void OnResponse(gint response_id);

  static void OnJobCompletedThunk(GtkPrintJob* print_job,
                                  gpointer user_data,
                                  GError* error) {
    reinterpret_cast<PrintDialogGtk*>(user_data)->OnJobCompleted(print_job,
                                                                 error);
  }
  void OnJobCompleted(GtkPrintJob* job, GError* error);

  FilePath path_to_pdf_;

  GtkWidget* dialog_;

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(PrintDialogGtk);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
