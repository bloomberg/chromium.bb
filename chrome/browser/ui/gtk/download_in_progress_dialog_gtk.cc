// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/download_in_progress_dialog_gtk.h"

#include <gtk/gtk.h>

#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

DownloadInProgressDialogGtk::DownloadInProgressDialogGtk(Browser* browser)
    : browser_(browser) {
  int download_count = browser->profile()->GetDownloadManager()->
      in_progress_count();

  std::string warning_text;
  std::string explanation_text;
  std::string ok_button_text;
  std::string cancel_button_text;
  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  if (download_count == 1) {
    warning_text =
        l10n_util::GetStringFUTF8(IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_WARNING,
                                  product_name);
    explanation_text =
        l10n_util::GetStringFUTF8(
            IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_EXPLANATION,
            product_name);
    ok_button_text = l10n_util::GetStringUTF8(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_OK_BUTTON_LABEL);
    cancel_button_text = l10n_util::GetStringUTF8(
        IDS_SINGLE_DOWNLOAD_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);
  } else {
    warning_text =
        l10n_util::GetStringFUTF8(IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_WARNING,
                                  product_name,
                                  base::IntToString16(download_count));
    explanation_text =
        l10n_util::GetStringFUTF8(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION, product_name);
    ok_button_text = l10n_util::GetStringUTF8(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_OK_BUTTON_LABEL);
    cancel_button_text = l10n_util::GetStringUTF8(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);
  }

  GtkWidget* dialog = gtk_message_dialog_new(
      browser_->window()->GetNativeHandle(),
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL),
      GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_NONE,
      "%s",
      warning_text.c_str());
  gtk_util::AddButtonToDialog(dialog,
      cancel_button_text.c_str(),
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
  gtk_util::AddButtonToDialog(dialog,
      ok_button_text.c_str(),
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);

  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                           "%s",
                                           explanation_text.c_str());

  g_signal_connect(dialog, "response", G_CALLBACK(OnResponseThunk), this);

  gtk_widget_show_all(dialog);
}

void DownloadInProgressDialogGtk::OnResponse(GtkWidget* widget,
                                             int response) {
  gtk_widget_destroy(widget);
  browser_->InProgressDownloadResponse(response == GTK_RESPONSE_ACCEPT);
  delete this;
}
