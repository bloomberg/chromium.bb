// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_protocol_dialog.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/external_protocol_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

namespace {

const int kMessageWidth = 400;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
// This should be kept in sync with RunExternalProtocolDialogViews in
// external_protocol_dialog_views_mac.mm.
// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id,
    ui::PageTransition page_transition, bool has_user_gesture) {
  std::unique_ptr<ExternalProtocolDialogDelegate> delegate(
      new ExternalProtocolDialogDelegate(url, render_process_host_id,
                                         routing_id));
  if (delegate->program_name().empty()) {
    // ShellExecute won't do anything. Don't bother warning the user.
    return;
  }

  // Windowing system takes ownership.
  new ExternalProtocolDialog(std::move(delegate), render_process_host_id,
                             routing_id);
}
#endif  // !OS_MACOSX || MAC_VIEWS_BROWSER

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
  return delegate_->GetDialogButtonLabel(button);
}

base::string16 ExternalProtocolDialog::GetWindowTitle() const {
  return delegate_->GetTitleText();
}

void ExternalProtocolDialog::DeleteDelegate() {
  delete this;
}

bool ExternalProtocolDialog::Cancel() {
  bool is_checked = message_box_view_->IsCheckBoxSelected();
  delegate_->DoCancel(delegate_->url(), is_checked);

  ExternalProtocolHandler::RecordCheckboxStateMetrics(is_checked);
  ExternalProtocolHandler::RecordHandleStateMetrics(
      is_checked, ExternalProtocolHandler::BLOCK);

  // Returning true closes the dialog.
  return true;
}

bool ExternalProtocolDialog::Accept() {
  // We record how long it takes the user to accept an external protocol.  If
  // users start accepting these dialogs too quickly, we should worry about
  // clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                           base::TimeTicks::Now() - creation_time_);

  bool is_checked = message_box_view_->IsCheckBoxSelected();
  ExternalProtocolHandler::RecordCheckboxStateMetrics(is_checked);
  ExternalProtocolHandler::RecordHandleStateMetrics(
      is_checked, ExternalProtocolHandler::DONT_BLOCK);

  delegate_->DoAccept(delegate_->url(), is_checked);

  // Returning true closes the dialog.
  return true;
}

bool ExternalProtocolDialog::Close() {
  // If the user dismisses the dialog without interacting with the buttons (e.g.
  // via pressing Esc or the X), act as though they cancelled the request, but
  // ignore the checkbox state. This ensures that if they check the checkbox but
  // dismiss the dialog, we don't stop prompting them forever.
  delegate_->DoCancel(delegate_->url(), false);
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

ui::ModalType ExternalProtocolDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, private:

ExternalProtocolDialog::ExternalProtocolDialog(
    std::unique_ptr<const ProtocolDialogDelegate> delegate,
    int render_process_host_id,
    int routing_id)
    : delegate_(std::move(delegate)),
      render_process_host_id_(render_process_host_id),
      routing_id_(routing_id),
      creation_time_(base::TimeTicks::Now()) {
  views::MessageBoxView::InitParams params(delegate_->GetMessageText());
  params.message_width = kMessageWidth;
  message_box_view_ = new views::MessageBoxView(params);
  message_box_view_->SetCheckBoxLabel(delegate_->GetCheckboxText());

  WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id_, routing_id_);
  // Only launch the dialog if there is a web contents associated with the
  // request.
  if (web_contents)
    constrained_window::ShowWebModalDialogViews(this, web_contents);
}
