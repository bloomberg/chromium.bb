// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_gtk.h"

#include <gtk/gtkprintjob.h>
#include <gtk/gtkprintunixdialog.h>
#include <gtk/gtkpagesetupunixdialog.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"

namespace {

// This is a temporary infobar designed to help gauge how many users are trying
// to print to printers that don't support PDF.
class PdfUnsupportedInfoBarDelegate : public LinkInfoBarDelegate {
 public:
  explicit PdfUnsupportedInfoBarDelegate(Browser* browser)
     : LinkInfoBarDelegate(NULL),
       browser_(browser) {
  }

  ~PdfUnsupportedInfoBarDelegate() {}

  virtual std::wstring GetMessageTextWithOffset(size_t* link_offset) const {
    std::wstring message(L"Oops! Your printer does not support PDF. Please "
                         L"report this to us .");
    *link_offset = message.length() - 1;
    return message;
  }

  virtual std::wstring GetLinkText() const {
    return std::wstring(L"here");
  }

  virtual Type GetInfoBarType() {
    return ERROR_TYPE;
  }

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
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(&PrintDialogGtk::CreateDialogImpl, path));
}

// static
void PrintDialogGtk::CreateDialogImpl(const FilePath& path) {
  new PrintDialogGtk(path);
}

PrintDialogGtk::PrintDialogGtk(const FilePath& path_to_pdf)
    : path_to_pdf_(path_to_pdf),
      browser_(BrowserList::GetLastActive()) {
  GtkWindow* parent = browser_->window()->GetNativeHandle();

  // TODO(estade): We need a window title here.
  dialog_ = gtk_print_unix_dialog_new(NULL, parent);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  gtk_widget_show_all(dialog_);
}

PrintDialogGtk::~PrintDialogGtk() {
}

void PrintDialogGtk::OnResponse(gint response_id) {
  gtk_widget_hide(dialog_);

  switch (response_id) {
    case GTK_RESPONSE_OK: {
      GtkPrinter* printer =
          gtk_print_unix_dialog_get_selected_printer(
              GTK_PRINT_UNIX_DIALOG(dialog_));
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

void PrintDialogGtk::OnJobCompleted(GtkPrintJob* job, GError* error) {
  gtk_widget_destroy(dialog_);

  if (error)
    LOG(ERROR) << "Printing failed: " << error->message;

  if (job)
    g_object_unref(job);

  file_util::Delete(path_to_pdf_, false);

  delete this;
}
