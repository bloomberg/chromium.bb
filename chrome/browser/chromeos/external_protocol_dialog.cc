// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_protocol_dialog.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using content::WebContents;

namespace {

const int kMessageWidth = 400;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolHandler

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url,
    int render_process_host_id,
    int routing_id,
    ui::PageTransition page_transition,
    bool has_user_gesture) {
  WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id, routing_id);
  new ExternalProtocolDialog(web_contents, url);
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

base::string16 ExternalProtocolDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT);
}

base::string16 ExternalProtocolDialog::GetWindowTitle() const {
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

const views::Widget* ExternalProtocolDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

views::Widget* ExternalProtocolDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, private:

ExternalProtocolDialog::ExternalProtocolDialog(WebContents* web_contents,
                                               const GURL& url)
    : creation_time_(base::TimeTicks::Now()),
      scheme_(url.scheme()) {
  const size_t kMaxUrlWithoutSchemeSize = 256;
  base::string16 elided_url_without_scheme;
  gfx::ElideString(base::ASCIIToUTF16(url.possibly_invalid_spec()),
      kMaxUrlWithoutSchemeSize, &elided_url_without_scheme);

  views::MessageBoxView::InitParams params(
      l10n_util::GetStringFUTF16(IDS_EXTERNAL_PROTOCOL_INFORMATION,
      base::ASCIIToUTF16(url.scheme() + ":"),
      elided_url_without_scheme) + base::ASCIIToUTF16("\n\n"));
  params.message_width = kMessageWidth;
  message_box_view_ = new views::MessageBoxView(params);
  message_box_view_->SetCheckBoxLabel(
      l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT));

  gfx::NativeWindow parent_window;
  if (web_contents) {
    parent_window = web_contents->GetTopLevelNativeWindow();
  } else {
    // Dialog is top level if we don't have a web_contents associated with us.
    parent_window = NULL;
  }
  views::DialogDelegate::CreateDialogWidget(this, NULL, parent_window)->Show();
}
