// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_gtk.h"

#include <gtk/gtkprintjob.h>
#include <gtk/gtkprintunixdialog.h>
#include <gtk/gtkpagesetupunixdialog.h>

#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/lazy_instance.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"

namespace {

PrintDialogGtk* g_print_dialog = NULL;

// Used to make accesses to the above thread safe.
Lock& DialogLock() {
  static base::LazyInstance<Lock> dialog_lock(base::LINKER_INITIALIZED);
  return dialog_lock.Get();
}

// This is a temporary infobar designed to help gauge how many users are trying
// to print to printers that don't support PDF.
class PdfUnsupportedInfoBarDelegate : public LinkInfoBarDelegate {
 public:
  explicit PdfUnsupportedInfoBarDelegate(Browser* browser)
     : LinkInfoBarDelegate(NULL),
       browser_(browser) {
  }

  virtual ~PdfUnsupportedInfoBarDelegate() {}

  virtual string16 GetMessageTextWithOffset(size_t* link_offset) const {
    string16 message = UTF8ToUTF16("Oops! Your printer does not support PDF. "
                                   "Please report this to us.");
    *link_offset = message.length() - 1;
    return message;
  }

  virtual string16 GetLinkText() const {
    return UTF8ToUTF16("here");
  }

  virtual Type GetInfoBarType() { return WARNING_TYPE; }

  virtual bool LinkClicked(WindowOpenDisposition disposition) {
    browser_->OpenURL(
        GURL("http://code.google.com/p/chromium/issues/detail?id=22027"),
        GURL(), NEW_FOREGROUND_TAB, PageTransition::TYPED);
    return true;
  }

 private:
  Browser* browser_;
};

}  // namespace

// static
void PrintDialogGtk::CreatePrintDialogForPdf(const FilePath& path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&PrintDialogGtk::CreateDialogImpl, path));
}

// static
bool PrintDialogGtk::DialogShowing() {
  AutoLock lock(DialogLock());
  return !!g_print_dialog;
}

// static
void PrintDialogGtk::CreateDialogImpl(const FilePath& path) {
  // Only show one print dialog at once. This is to prevent a page from
  // locking up the system with
  //
  //   while(true){print();}
  AutoLock lock(DialogLock());
  if (g_print_dialog) {
    // Clean up the temporary file.
    base::FileUtilProxy::Delete(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        path, false, NULL);
    return;
  }

  g_print_dialog = new PrintDialogGtk(path);
}

PrintDialogGtk::PrintDialogGtk(const FilePath& path_to_pdf)
    : path_to_pdf_(path_to_pdf),
      browser_(BrowserList::GetLastActive()) {
  GtkWindow* parent = browser_->window()->GetNativeHandle();

  // TODO(estade): We need a window title here.
  dialog_ = gtk_print_unix_dialog_new(NULL, parent);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  gtk_widget_show(dialog_);
}

PrintDialogGtk::~PrintDialogGtk() {
  AutoLock lock(DialogLock());
  DCHECK_EQ(this, g_print_dialog);
  g_print_dialog = NULL;
}

void PrintDialogGtk::OnResponse(GtkWidget* dialog, gint response_id) {
  gtk_widget_hide(dialog_);

  switch (response_id) {
    case GTK_RESPONSE_OK: {
      GtkPrinter* printer =
          gtk_print_unix_dialog_get_selected_printer(
              GTK_PRINT_UNIX_DIALOG(dialog_));
      // Attempt to track down bug 70166.
      CHECK(printer != NULL);
      if (!gtk_printer_accepts_pdf(printer)) {
        browser_->GetSelectedTabContents()->AddInfoBar(
            new PdfUnsupportedInfoBarDelegate(browser_));
        break;
      }

      GtkPrintSettings* settings =
          gtk_print_unix_dialog_get_settings(
              GTK_PRINT_UNIX_DIALOG(dialog_));
      GtkPageSetup* setup = gtk_print_unix_dialog_get_page_setup(
              GTK_PRINT_UNIX_DIALOG(dialog_));

      GtkPrintJob* job =
          gtk_print_job_new(path_to_pdf_.value().c_str(), printer,
                            settings, setup);
      gtk_print_job_set_source_file(job, path_to_pdf_.value().c_str(), NULL);
      gtk_print_job_send(job, OnJobCompletedThunk, this, NULL);
      g_object_unref(settings);
      // Success; return early.
      return;
    }
    case GTK_RESPONSE_DELETE_EVENT:  // Fall through.
    case GTK_RESPONSE_CANCEL: {
      break;
    }
    case GTK_RESPONSE_APPLY:
    default: {
      NOTREACHED();
    }
  }

  // Delete this dialog.
  OnJobCompleted(NULL, NULL);
}

void PrintDialogGtk::OnJobCompletedThunk(GtkPrintJob* print_job,
                                         gpointer user_data,
                                         GError* error) {
  reinterpret_cast<PrintDialogGtk*>(user_data)->OnJobCompleted(print_job,
                                                               error);
}

void PrintDialogGtk::OnJobCompleted(GtkPrintJob* job, GError* error) {
  gtk_widget_destroy(dialog_);

  if (error)
    LOG(ERROR) << "Printing failed: " << error->message;

  if (job)
    g_object_unref(job);

  base::FileUtilProxy::Delete(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      path_to_pdf_,
      false,
      NULL);

  delete this;
}
