// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_gtk.h"

#include <fcntl.h>
#include <gtk/gtkpagesetupunixdialog.h>
#include <gtk/gtkprintjob.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/browser_window.h"
#include "printing/print_settings_initializer_gtk.h"

// static
void* PrintDialogGtk::CreatePrintDialog(
    PrintingContextCairo::PrintSettingsCallback* callback,
    PrintingContextCairo* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PrintDialogGtk* dialog = new PrintDialogGtk(callback, context);
  return dialog;
}

// static
void PrintDialogGtk::PrintDocument(void* print_dialog,
                                   const NativeMetafile* metafile,
                                   const string16& document_name) {
  PrintDialogGtk* dialog = static_cast<PrintDialogGtk*>(print_dialog);

  scoped_ptr<base::WaitableEvent> event(new base::WaitableEvent(false, false));
  dialog->set_save_document_event(event.get());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(dialog,
                        &PrintDialogGtk::SaveDocumentToDisk,
                        metafile,
                        document_name));
  // Wait for SaveDocumentToDisk() to finish.
  event->Wait();
}

PrintDialogGtk::PrintDialogGtk(
    PrintingContextCairo::PrintSettingsCallback* callback,
    PrintingContextCairo* context)
    : callback_(callback),
      context_(context),
      dialog_(NULL),
      page_setup_(NULL),
      printer_(NULL),
      gtk_settings_(NULL),
      save_document_event_(NULL) {
  // Manual AddRef since PrintDialogGtk manages its own lifetime.
  AddRef();

  GtkWindow* parent = BrowserList::GetLastActive()->window()->GetNativeHandle();

  // TODO(estade): We need a window title here.
  dialog_ = gtk_print_unix_dialog_new(NULL, parent);
  // Set modal so user cannot focus the same tab and press print again.
  gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);

  // Since we only generate PDF, only show printers that support PDF.
  // TODO(thestig) Add more capabilities to support?
  GtkPrintCapabilities cap = static_cast<GtkPrintCapabilities>(
      GTK_PRINT_CAPABILITY_GENERATE_PDF |
      GTK_PRINT_CAPABILITY_PAGE_SET |
      GTK_PRINT_CAPABILITY_COPIES |
      GTK_PRINT_CAPABILITY_COLLATE |
      GTK_PRINT_CAPABILITY_REVERSE);
  gtk_print_unix_dialog_set_manual_capabilities(GTK_PRINT_UNIX_DIALOG(dialog_),
                                                cap);
#if GTK_CHECK_VERSION(2, 18, 0)
  gtk_print_unix_dialog_set_embed_page_setup(GTK_PRINT_UNIX_DIALOG(dialog_),
                                             TRUE);
#endif
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  gtk_widget_show(dialog_);
}

PrintDialogGtk::~PrintDialogGtk() {
  gtk_widget_destroy(dialog_);
  dialog_ = NULL;
  page_setup_ = NULL;
  printer_ = NULL;
  if (gtk_settings_) {
    g_object_unref(gtk_settings_);
    gtk_settings_ = NULL;
  }
}

void PrintDialogGtk::OnResponse(GtkWidget* dialog, gint response_id) {
  gtk_widget_hide(dialog_);

  switch (response_id) {
    case GTK_RESPONSE_OK: {
      // |gtk_settings_| is a new object.
      gtk_settings_ = gtk_print_unix_dialog_get_settings(
          GTK_PRINT_UNIX_DIALOG(dialog_));
      // |printer_| and |page_setup_| are owned by |dialog_|.
      page_setup_ = gtk_print_unix_dialog_get_page_setup(
          GTK_PRINT_UNIX_DIALOG(dialog_));
      printer_ = gtk_print_unix_dialog_get_selected_printer(
          GTK_PRINT_UNIX_DIALOG(dialog_));

      printing::PageRanges ranges_vector;
      gint num_ranges;
      GtkPageRange* gtk_range =
          gtk_print_settings_get_page_ranges(gtk_settings_, &num_ranges);
      if (gtk_range) {
        for (int i = 0; i < num_ranges; ++i) {
          printing::PageRange* range = new printing::PageRange;
          range->from = gtk_range[i].start;
          range->to = gtk_range[i].end;
          ranges_vector.push_back(*range);
        }
        g_free(gtk_range);
      }

      printing::PrintSettings settings;
      printing::PrintSettingsInitializerGtk::InitPrintSettings(
          gtk_settings_, page_setup_, ranges_vector, false, &settings);
      context_->InitWithSettings(settings);
      callback_->Run(PrintingContextCairo::OK);
      return;
    }
    case GTK_RESPONSE_DELETE_EVENT:  // Fall through.
    case GTK_RESPONSE_CANCEL: {
      callback_->Run(PrintingContextCairo::CANCEL);
      Release();
      return;
    }
    case GTK_RESPONSE_APPLY:
    default: {
      NOTREACHED();
    }
  }
}

void PrintDialogGtk::SaveDocumentToDisk(const NativeMetafile* metafile,
                                        const string16& document_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool error = false;
  if (!file_util::CreateTemporaryFile(&path_to_pdf_)) {
    LOG(ERROR) << "Creating temporary file failed";
    error = true;
  }

  if (!error) {
    base::FileDescriptor temp_file_fd;
    temp_file_fd.fd = open(path_to_pdf_.value().c_str(), O_WRONLY);
    temp_file_fd.auto_close = true;
    if (!metafile->SaveTo(temp_file_fd)) {
      LOG(ERROR) << "Saving metafile failed";
      file_util::Delete(path_to_pdf_, false);
      error = true;
    }
  }

  // Done saving, let PrintDialogGtk::PrintDocument() continue.
  save_document_event_->Signal();

  if (error) {
    Release();
  } else {
    // No errors, continue printing.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &PrintDialogGtk::SendDocumentToPrinter,
                          document_name));
  }
}

void PrintDialogGtk::SendDocumentToPrinter(const string16& document_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GtkPrintJob* print_job = gtk_print_job_new(
      UTF16ToUTF8(document_name).c_str(),
      printer_,
      gtk_settings_,
      page_setup_);
  gtk_print_job_set_source_file(print_job, path_to_pdf_.value().c_str(), NULL);
  gtk_print_job_send(print_job, OnJobCompletedThunk, this, NULL);
}

// static
void PrintDialogGtk::OnJobCompletedThunk(GtkPrintJob* print_job,
                                         gpointer user_data,
                                         GError* error) {
  static_cast<PrintDialogGtk*>(user_data)->OnJobCompleted(print_job, error);
}

void PrintDialogGtk::OnJobCompleted(GtkPrintJob* print_job, GError* error) {
  if (error)
    LOG(ERROR) << "Printing failed: " << error->message;
  if (print_job)
    g_object_unref(print_job);
  base::FileUtilProxy::Delete(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      path_to_pdf_,
      false,
      NULL);
  // Printing finished.
  Release();
}

void PrintDialogGtk::set_save_document_event(base::WaitableEvent* event) {
  DCHECK(event);
  DCHECK(!save_document_event_);
  save_document_event_ = event;
}
