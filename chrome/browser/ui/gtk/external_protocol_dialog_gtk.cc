// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/external_protocol_dialog_gtk.h"

#include <gtk/gtk.h>

#include <string>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/text/text_elider.h"

namespace {

const int kMessageWidth = 400;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  new ExternalProtocolDialogGtk(url);
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialogGtk

ExternalProtocolDialogGtk::ExternalProtocolDialogGtk(const GURL& url)
    : url_(url),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_EXTERNAL_PROTOCOL_TITLE).c_str(),
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      NULL);

  // Add the response buttons.
  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(
          IDS_EXTERNAL_PROTOCOL_CANCEL_BUTTON_TEXT).c_str(),
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
  gtk_util::AddButtonToDialog(dialog_,
      l10n_util::GetStringUTF8(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT).c_str(),
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);

  // Construct the message text.
  const int kMaxUrlWithoutSchemeSize = 256;
  const int kMaxCommandSize = 256;
  std::wstring elided_url_without_scheme;
  std::wstring elided_command;
  ui::ElideString(ASCIIToWide(url.possibly_invalid_spec()),
      kMaxUrlWithoutSchemeSize, &elided_url_without_scheme);
  ui::ElideString(ASCIIToWide(std::string("xdg-open ") + url.spec()),
      kMaxCommandSize, &elided_command);

  std::string message_text = l10n_util::GetStringFUTF8(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      ASCIIToUTF16(url.scheme() + ":"),
      WideToUTF16(elided_url_without_scheme)) + "\n\n";

  message_text += l10n_util::GetStringFUTF8(
      IDS_EXTERNAL_PROTOCOL_APPLICATION_TO_LAUNCH,
      WideToUTF16(elided_command)) + "\n\n";

  message_text += l10n_util::GetStringUTF8(IDS_EXTERNAL_PROTOCOL_WARNING);

  // Create the content-holding vbox.
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(vbox),
                                 gtk_util::kContentAreaBorder);

  // Add the message text.
  GtkWidget* label = gtk_label_new(message_text.c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(label, kMessageWidth, -1);
  gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

  // Add the checkbox.
  checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), checkbox_,
                     FALSE, FALSE, 0);

  // Add our vbox to the dialog.
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), vbox,
                     FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnDialogResponseThunk), this);

  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
  gtk_widget_show_all(dialog_);
}

void ExternalProtocolDialogGtk::OnDialogResponse(GtkWidget* widget,
                                                 int response) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_))) {
    if (response == GTK_RESPONSE_ACCEPT) {
      ExternalProtocolHandler::SetBlockState(
          url_.scheme(), ExternalProtocolHandler::DONT_BLOCK);
    } else if (response == GTK_RESPONSE_REJECT) {
      ExternalProtocolHandler::SetBlockState(
          url_.scheme(), ExternalProtocolHandler::BLOCK);
    }
    // If the response is GTK_RESPONSE_DELETE, triggered by the user closing
    // the dialog, do nothing.
  }

  if (response == GTK_RESPONSE_ACCEPT) {
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                             base::TimeTicks::Now() - creation_time_);

    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url_);
  }

  gtk_widget_destroy(dialog_);
  delete this;
}
