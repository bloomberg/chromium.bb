// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/protocol_dialog_gtk.h"

#include <gtk/gtk.h>

#include <string>

#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/external_protocol_dialog_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const int kMessageWidth = 400;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  new ProtocolDialogGtk(scoped_ptr<const ProtocolDialogDelegate>(
        new ExternalProtocolDialogDelegate(url)));
}

///////////////////////////////////////////////////////////////////////////////
// ProtocolDialogGtk

ProtocolDialogGtk::ProtocolDialogGtk(
    scoped_ptr<const ProtocolDialogDelegate> delegate)
    : delegate_(delegate.Pass()),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());

  dialog_ = gtk_dialog_new_with_buttons(
      UTF16ToUTF8(delegate_->GetTitleText()).c_str(),
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

  // Create the content-holding vbox.
  GtkWidget* vbox = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(vbox),
                                 ui::kContentAreaBorder);

  // Add the message text.
  GtkWidget* label = gtk_label_new(
      UTF16ToUTF8(delegate_->GetMessageText()).c_str());
  gtk_util::SetLabelWidth(label, kMessageWidth);
  gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

  // Add the checkbox.
  checkbox_ = gtk_check_button_new_with_label(
      UTF16ToUTF8(delegate_->GetCheckboxText()).c_str());
  gtk_box_pack_start(GTK_BOX(vbox), checkbox_,
                     FALSE, FALSE, 0);

  // Add our vbox to the dialog.
  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
  gtk_box_pack_start(GTK_BOX(content_area), vbox, FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
  gtk_widget_show_all(dialog_);
}

ProtocolDialogGtk::~ProtocolDialogGtk() {
}

void ProtocolDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  bool checkbox = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_));
  if (response_id == GTK_RESPONSE_ACCEPT) {
    delegate_->DoAccept(delegate_->url(), checkbox);
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                             base::TimeTicks::Now() - creation_time_);
  } else if (response_id == GTK_RESPONSE_REJECT) {
    delegate_->DoCancel(delegate_->url(), checkbox);
  }
  // If the response is GTK_RESPONSE_DELETE, triggered by the user closing
  // the dialog, do nothing.

  gtk_widget_destroy(dialog_);
  delete this;
}
