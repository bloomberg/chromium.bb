// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_in_progress_dialog_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/string16.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

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
                                  product_name, IntToString16(download_count));
    explanation_text =
        l10n_util::GetStringFUTF8(
            IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_EXPLANATION, product_name);
    ok_button_text = l10n_util::GetStringUTF8(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_OK_BUTTON_LABEL);
    cancel_button_text = l10n_util::GetStringUTF8(
        IDS_MULTIPLE_DOWNLOADS_REMOVE_CONFIRM_CANCEL_BUTTON_LABEL);
  }

  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      UTF16ToUTF8(product_name).c_str(),
      browser_->window()->GetNativeHandle(),
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      NULL);
  gtk_util::AddButtonToDialog(dialog,
      cancel_button_text.c_str(),
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
  gtk_util::AddButtonToDialog(dialog,
      ok_button_text.c_str(),
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;

  // There are two lines of text: the bold warning label and the text
  // explanation label. Neither one wraps.
  GtkWidget* warning_label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(warning_label),
                       (std::string("<b>") + warning_text + "</b>").c_str());
  gtk_misc_set_alignment(GTK_MISC(warning_label), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER(content_area), warning_label);

  // Spacing line.
  gtk_container_add(GTK_CONTAINER(content_area), gtk_label_new(NULL));

  GtkWidget* explanation_label = gtk_label_new(explanation_text.c_str());
  gtk_misc_set_alignment(GTK_MISC(explanation_label), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER(content_area), explanation_label);

  // Spacing line.
  gtk_container_add(GTK_CONTAINER(content_area), gtk_label_new(NULL));

  g_signal_connect(dialog, "response", G_CALLBACK(OnResponse), this);

  gtk_widget_show_all(dialog);
}

void DownloadInProgressDialogGtk::OnResponse(
    GtkWidget* widget,
    int response,
    DownloadInProgressDialogGtk* dialog) {
  gtk_widget_destroy(widget);
  dialog->browser_->InProgressDownloadResponse(response == GTK_RESPONSE_ACCEPT);
  delete dialog;
}
