// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
#define CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
#pragma once

#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "printing/print_dialog_gtk_interface.h"
#include "printing/printing_context_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

namespace printing {
class Metafile;
class PrintSettings;
}

using printing::PrintingContextGtk;

// Needs to be freed on the UI thread to clean up its GTK members variables.
class PrintDialogGtk
    : public printing::PrintDialogGtkInterface,
      public base::RefCountedThreadSafe<
          PrintDialogGtk, content::BrowserThread::DeleteOnUIThread> {
 public:
  // Creates and returns a print dialog.
  static printing::PrintDialogGtkInterface* CreatePrintDialog(
      PrintingContextGtk* context);

  // printing::PrintDialogGtkInterface implementation.
  virtual void UseDefaultSettings() OVERRIDE;
  virtual bool UpdateSettings(const base::DictionaryValue& job_settings,
                              const printing::PageRanges& ranges,
                              printing::PrintSettings* settings) OVERRIDE;
  virtual void ShowDialog(
      const PrintingContextGtk::PrintSettingsCallback& callback) OVERRIDE;
  virtual void PrintDocument(const printing::Metafile* metafile,
                             const string16& document_name) OVERRIDE;
  virtual void AddRefToDialog() OVERRIDE;
  virtual void ReleaseDialog() OVERRIDE;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class DeleteTask<PrintDialogGtk>;

  explicit PrintDialogGtk(PrintingContextGtk* context);
  virtual ~PrintDialogGtk();

  // Handles dialog response.
  CHROMEGTK_CALLBACK_1(PrintDialogGtk, void, OnResponse, int);

  // Prints document named |document_name|.
  void SendDocumentToPrinter(const string16& document_name);

  // Handles print job response.
  static void OnJobCompletedThunk(GtkPrintJob* print_job,
                                  gpointer user_data,
                                  GError* error);
  void OnJobCompleted(GtkPrintJob* print_job, GError* error);

  // Helper function for initializing |context_|'s PrintSettings with a given
  // set of |page_ranges| and |settings|.
  void InitPrintSettings(const printing::PageRanges& page_ranges,
                         printing::PrintSettings* settings);

  // Printing dialog callback.
  PrintingContextGtk::PrintSettingsCallback callback_;
  PrintingContextGtk* context_;

  // Print dialog settings. PrintDialogGtk owns |dialog_| and holds references
  // to the other objects.
  GtkWidget* dialog_;
  GtkPrintSettings* gtk_settings_;
  GtkPageSetup* page_setup_;
  GtkPrinter* printer_;

  FilePath path_to_pdf_;

  DISALLOW_COPY_AND_ASSIGN(PrintDialogGtk);
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_DIALOG_GTK_H_
