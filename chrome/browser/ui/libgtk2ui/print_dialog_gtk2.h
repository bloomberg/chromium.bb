// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_PRINT_DIALOG_GTK2_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_PRINT_DIALOG_GTK2_H_

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_signal.h"
#include "content/public/browser/browser_thread.h"
#include "printing/print_dialog_gtk_interface.h"
#include "printing/printing_context_linux.h"
#include "ui/aura/window_observer.h"

namespace printing {
class Metafile;
class PrintSettings;
}

using printing::PrintingContextLinux;

// Needs to be freed on the UI thread to clean up its GTK members variables.
class PrintDialogGtk2
    : public printing::PrintDialogGtkInterface,
      public base::RefCountedThreadSafe<
          PrintDialogGtk2, content::BrowserThread::DeleteOnUIThread>,
      public aura::WindowObserver {
 public:
  // Creates and returns a print dialog.
  static printing::PrintDialogGtkInterface* CreatePrintDialog(
      PrintingContextLinux* context);

  // printing::PrintDialogGtkInterface implementation.
  virtual void UseDefaultSettings() OVERRIDE;
  virtual bool UpdateSettings(printing::PrintSettings* settings) OVERRIDE;
  virtual void ShowDialog(
      gfx::NativeView parent_view,
      bool has_selection,
      const PrintingContextLinux::PrintSettingsCallback& callback) OVERRIDE;
  virtual void PrintDocument(const printing::Metafile* metafile,
                             const base::string16& document_name) OVERRIDE;
  virtual void AddRefToDialog() OVERRIDE;
  virtual void ReleaseDialog() OVERRIDE;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<PrintDialogGtk2>;

  explicit PrintDialogGtk2(PrintingContextLinux* context);
  virtual ~PrintDialogGtk2();

  // Handles dialog response.
  CHROMEGTK_CALLBACK_1(PrintDialogGtk2, void, OnResponse, int);

  // Prints document named |document_name|.
  void SendDocumentToPrinter(const base::string16& document_name);

  // Handles print job response.
  static void OnJobCompletedThunk(GtkPrintJob* print_job,
                                  gpointer user_data,
                                  GError* error);
  void OnJobCompleted(GtkPrintJob* print_job, GError* error);

  // Helper function for initializing |context_|'s PrintSettings with a given
  // |settings|.
  void InitPrintSettings(printing::PrintSettings* settings);

  // aura::WindowObserver implementation.
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Printing dialog callback.
  PrintingContextLinux::PrintSettingsCallback callback_;
  PrintingContextLinux* context_;

  // Print dialog settings. PrintDialogGtk2 owns |dialog_| and holds references
  // to the other objects.
  GtkWidget* dialog_;
  GtkPrintSettings* gtk_settings_;
  GtkPageSetup* page_setup_;
  GtkPrinter* printer_;

  base::FilePath path_to_pdf_;

  DISALLOW_COPY_AND_ASSIGN(PrintDialogGtk2);
};

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_PRINT_DIALOG_GTK2_H_
