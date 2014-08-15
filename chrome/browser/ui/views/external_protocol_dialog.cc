// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_protocol_dialog.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/external_protocol_dialog_delegate.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace {

const int kMessageWidth = 400;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  scoped_ptr<ExternalProtocolDialogDelegate> delegate(
      new ExternalProtocolDialogDelegate(url,
                                         render_process_host_id,
                                         routing_id));
  if (delegate->program_name().empty()) {
    // ShellExecute won't do anything. Don't bother warning the user.
    return;
  }

  // Windowing system takes ownership.
  new ExternalProtocolDialog(delegate.PassAs<const ProtocolDialogDelegate>(),
                             render_process_host_id,
                             routing_id);
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog

ExternalProtocolDialog::~ExternalProtocolDialog() {
}

//////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, views::DialogDelegate implementation:

int ExternalProtocolDialog::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 ExternalProtocolDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT);
  else
    return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CANCEL_BUTTON_TEXT);
}

base::string16 ExternalProtocolDialog::GetWindowTitle() const {
  return delegate_->GetTitleText();
}

void ExternalProtocolDialog::DeleteDelegate() {
  delete this;
}

bool ExternalProtocolDialog::Cancel() {
  // We also get called back here if the user closes the dialog or presses
  // escape. In these cases it would be preferable to ignore the state of the
  // check box but MessageBox doesn't distinguish this from pressing the cancel
  // button.
  delegate_->DoCancel(delegate_->url(),
                      message_box_view_->IsCheckBoxSelected());

  // Returning true closes the dialog.
  return true;
}

bool ExternalProtocolDialog::Accept() {
  // We record how long it takes the user to accept an external protocol.  If
  // users start accepting these dialogs too quickly, we should worry about
  // clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                           base::TimeTicks::Now() - creation_time_);

  delegate_->DoAccept(delegate_->url(),
                      message_box_view_->IsCheckBoxSelected());

  // Returning true closes the dialog.
  return true;
}

views::View* ExternalProtocolDialog::GetContentsView() {
  return message_box_view_;
}

views::Widget* ExternalProtocolDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* ExternalProtocolDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, private:

ExternalProtocolDialog::ExternalProtocolDialog(
    scoped_ptr<const ProtocolDialogDelegate> delegate,
    int render_process_host_id,
    int routing_id)
    : delegate_(delegate.Pass()),
      render_process_host_id_(render_process_host_id),
      routing_id_(routing_id),
      creation_time_(base::TimeTicks::Now()) {
  views::MessageBoxView::InitParams params(delegate_->GetMessageText());
  params.message_width = kMessageWidth;
  message_box_view_ = new views::MessageBoxView(params);
  message_box_view_->SetCheckBoxLabel(delegate_->GetCheckboxText());

  // Dialog is top level if we don't have a web_contents associated with us.
  WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id_, routing_id_);
  gfx::NativeWindow parent_window = NULL;
  if (web_contents)
    parent_window = web_contents->GetTopLevelNativeWindow();
  CreateBrowserModalDialogViews(this, parent_window)->Show();
}
