// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/external_protocol_dialog_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/histogram.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

const int kMessageWidth = 400;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  TabContents* tab_contents = tab_util::GetTabContentsByID(
      render_process_host_id, routing_id);
  new ExternalProtocolDialogGtk(url, tab_contents);
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialogGtk

ExternalProtocolDialogGtk::ExternalProtocolDialogGtk(
    const GURL& url, TabContents* tab_contents)
    : url_(url),
      creation_time_(base::Time::Now()) {
  GtkWindow* parent = tab_contents ?
                      tab_contents->view()->GetTopLevelNativeWindow() : NULL;

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_EXTERNAL_PROTOCOL_TITLE).c_str(),
      parent,
      (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_REJECT,
      NULL);

  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT).c_str(),
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);

  std::string message_text = l10n_util::GetStringFUTF8(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      ASCIIToUTF16(url.scheme() + ":"),
      ASCIIToUTF16(url.possibly_invalid_spec())) + "\n\n";

  message_text += l10n_util::GetStringFUTF8(
      IDS_EXTERNAL_PROTOCOL_APPLICATION_TO_LAUNCH,
      ASCIIToUTF16(std::string("xdg-open ") + url.spec())) + "\n\n";

  message_text += l10n_util::GetStringUTF8(IDS_EXTERNAL_PROTOCOL_WARNING);

  GtkWidget* label = gtk_label_new(message_text.c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(label, kMessageWidth, -1);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), label, TRUE, TRUE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnDialogResponse), this);

  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
  gtk_widget_show_all(dialog_);
}

void ExternalProtocolDialogGtk::OnDialogResponse(GtkWidget* widget,
    int response, ExternalProtocolDialogGtk* dialog) {
  if (response == GTK_RESPONSE_ACCEPT) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                             base::Time::Now() - dialog->creation_time_);

    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(dialog->url_);
  }

  gtk_widget_destroy(widget);
  delete dialog;
}
