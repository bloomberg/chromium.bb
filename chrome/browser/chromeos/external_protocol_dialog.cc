// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/external_protocol_dialog.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

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
  return MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring ExternalProtocolDialog::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  return l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT);
}

std::wstring ExternalProtocolDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_TITLE);
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

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, private:

ExternalProtocolDialog::ExternalProtocolDialog(TabContents* tab_contents,
                                               const GURL& url)
    : creation_time_(base::TimeTicks::Now()),
      scheme_(url.scheme()) {
  const int kMaxUrlWithoutSchemeSize = 256;
  std::wstring elided_url_without_scheme;
  ElideString(ASCIIToWide(url.possibly_invalid_spec()),
      kMaxUrlWithoutSchemeSize, &elided_url_without_scheme);

  std::wstring message_text = l10n_util::GetStringF(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      ASCIIToWide(url.scheme() + ":"),
      elided_url_without_scheme) + L"\n\n";

  message_box_view_ = new MessageBoxView(MessageBoxFlags::kIsConfirmMessageBox,
                                         message_text,
                                         std::wstring(),
                                         kMessageWidth);
  message_box_view_->SetCheckBoxLabel(
      l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT));

  gfx::NativeWindow parent_window;
  if (tab_contents) {
    parent_window = tab_contents->view()->GetTopLevelNativeWindow();
  } else {
    // Dialog is top level if we don't have a tab_contents associated with us.
    parent_window = NULL;
  }
  views::Window::CreateChromeWindow(parent_window, gfx::Rect(), this)->Show();
}
