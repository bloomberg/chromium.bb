// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_protocol_dialog.h"

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "ui/base/text/text_elider.h"
#include "views/controls/message_box_view.h"
#include "views/widget/widget.h"

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
  DCHECK(tab_contents);
  new ExternalProtocolDialog(tab_contents, url);
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog

ExternalProtocolDialog::~ExternalProtocolDialog() {
}

//////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, views::DialogDelegate implementation:

int ExternalProtocolDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

string16 ExternalProtocolDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT);
}

string16 ExternalProtocolDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_TITLE);
}

void ExternalProtocolDialog::DeleteDelegate() {
  delete this;
}

bool ExternalProtocolDialog::Accept() {
  if (message_box_view_->IsCheckBoxSelected()) {
    ExternalProtocolHandler::SetBlockState(
        scheme_, ExternalProtocolHandler::DONT_BLOCK);
  }
  // Returning true closes the dialog.
  return true;
}

views::View* ExternalProtocolDialog::GetContentsView() {
  return message_box_view_;
}

bool ExternalProtocolDialog::IsModal() const {
  return false;
}

const views::Widget* ExternalProtocolDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

views::Widget* ExternalProtocolDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, private:

ExternalProtocolDialog::ExternalProtocolDialog(TabContents* tab_contents,
                                               const GURL& url)
    : creation_time_(base::TimeTicks::Now()),
      scheme_(url.scheme()) {
  const int kMaxUrlWithoutSchemeSize = 256;
  string16 elided_url_without_scheme;
  ui::ElideString(ASCIIToUTF16(url.possibly_invalid_spec()),
      kMaxUrlWithoutSchemeSize, &elided_url_without_scheme);

  string16 message_text = l10n_util::GetStringFUTF16(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      ASCIIToUTF16(url.scheme() + ":"),
      elided_url_without_scheme) + ASCIIToUTF16("\n\n");

  message_box_view_ = new views::MessageBoxView(
      ui::MessageBoxFlags::kIsConfirmMessageBox,
      message_text,
      string16(),
      kMessageWidth);
  message_box_view_->SetCheckBoxLabel(
      l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT));

  gfx::NativeWindow parent_window;
  if (tab_contents) {
    parent_window = tab_contents->view()->GetTopLevelNativeWindow();
  } else {
    // Dialog is top level if we don't have a tab_contents associated with us.
    parent_window = NULL;
  }
  browser::CreateViewsWindow(parent_window, this)->Show();
}
