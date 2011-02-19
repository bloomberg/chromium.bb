// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
#define CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "printing/native_metafile.h"
#include "printing/printing_context_cairo.h"
#include "ui/base/gtk/gtk_signal.h"

namespace base {
class WaitableEvent;
}

using printing::NativeMetafile;
using printing::PrintingContextCairo;

class PrintDialogGtk : public base::RefCountedThreadSafe<PrintDialogGtk> {
 public:
  // Creates and returns a print dialog. The dialog will initialize the settings
  // for |context| and call |callback| to inform the print workflow of the
  // dialog results.
  static void* CreatePrintDialog(
      PrintingContextCairo::PrintSettingsCallback* callback,
      PrintingContextCairo* context);

  // Prints a document named |document_name| based on the settings in
  // |print_dialog|, with data from |metafile|.
  // Called from the print worker thread.
  static void PrintDocument(void* print_dialog,
                            const NativeMetafile* metafile,
                            const string16& document_name);

 private:
  friend class base::RefCountedThreadSafe<PrintDialogGtk>;

  PrintDialogGtk(PrintingContextCairo::PrintSettingsCallback* callback,
                 PrintingContextCairo* context);
  ~PrintDialogGtk();

  // Handles dialog response.
  CHROMEGTK_CALLBACK_1(PrintDialogGtk, void, OnResponse, gint);

  // Saves data in |metafile| to disk for document named |document_name|.
  void SaveDocumentToDisk(const NativeMetafile* metafile,
                          const string16& document_name);

  // Prints document named |document_name|.
  void SendDocumentToPrinter(const string16& document_name);

  // Handles print job response.
  static void OnJobCompletedThunk(GtkPrintJob* print_job,
                                  gpointer user_data,
                                  GError* error);
  void OnJobCompleted(GtkPrintJob* print_job, GError* error);

  void set_save_document_event(base::WaitableEvent* event);

  // Printing dialog callback.
  PrintingContextCairo::PrintSettingsCallback* callback_;
  PrintingContextCairo* context_;

  // Print dialog settings.
  GtkWidget* dialog_;
  GtkPageSetup* page_setup_;
  GtkPrinter* printer_;
  GtkPrintSettings* gtk_settings_;

  // Event to signal when save document finishes.
  base::WaitableEvent* save_document_event_;
  FilePath path_to_pdf_;

  DISALLOW_COPY_AND_ASSIGN(PrintDialogGtk);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
