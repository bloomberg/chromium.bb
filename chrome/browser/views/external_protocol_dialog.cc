// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/external_protocol_dialog.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
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
  std::wstring command =
      ExternalProtocolDialog::GetApplicationForProtocol(url);
  if (command.empty()) {
    // ShellExecute won't do anything. Don't bother warning the user.
    return;
  }
  TabContents* tab_contents = tab_util::GetTabContentsByID(
      render_process_host_id, routing_id);
  DCHECK(tab_contents);
  ExternalProtocolDialog* handler =
      new ExternalProtocolDialog(tab_contents, url, command);
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog

ExternalProtocolDialog::~ExternalProtocolDialog() {
}

//////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, views::DialogDelegate implementation:

int ExternalProtocolDialog::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring ExternalProtocolDialog::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT);
  else
    return l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_CANCEL_BUTTON_TEXT);
}

std::wstring ExternalProtocolDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_TITLE);
}

void ExternalProtocolDialog::DeleteDelegate() {
  delete this;
}

bool ExternalProtocolDialog::Cancel() {
  // We also get called back here if the user closes the dialog or presses
  // escape. In these cases it would be preferable to ignore the state of the
  // check box but MessageBox doesn't distinguish this from pressing the cancel
  // button.
  if (message_box_view_->IsCheckBoxSelected()) {
    ExternalProtocolHandler::SetBlockState(
        url_.scheme(), ExternalProtocolHandler::BLOCK);
  }

  // Returning true closes the dialog.
  return true;
}

bool ExternalProtocolDialog::Accept() {
  // We record how long it takes the user to accept an external protocol.  If
  // users start accepting these dialogs too quickly, we should worry about
  // clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.launch_url",
                           base::TimeTicks::Now() - creation_time_);

  if (message_box_view_->IsCheckBoxSelected()) {
    ExternalProtocolHandler::SetBlockState(
        url_.scheme(), ExternalProtocolHandler::DONT_BLOCK);
  }

  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url_);
  // Returning true closes the dialog.
  return true;
}

views::View* ExternalProtocolDialog::GetContentsView() {
  return message_box_view_;
}

///////////////////////////////////////////////////////////////////////////////
// ExternalProtocolDialog, private:

ExternalProtocolDialog::ExternalProtocolDialog(TabContents* tab_contents,
                                               const GURL& url,
                                               const std::wstring& command)
    : tab_contents_(tab_contents),
      url_(url),
      creation_time_(base::TimeTicks::Now()) {
  const int kMaxUrlWithoutSchemeSize = 256;
  const int kMaxCommandSize = 256;
  std::wstring elided_url_without_scheme;
  std::wstring elided_command;
  ElideString(ASCIIToWide(url.possibly_invalid_spec()),
      kMaxUrlWithoutSchemeSize, &elided_url_without_scheme);
  ElideString(command, kMaxCommandSize, &elided_command);

  std::wstring message_text = l10n_util::GetStringF(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      ASCIIToWide(url.scheme() + ":"),
      elided_url_without_scheme) + L"\n\n";

  message_text += l10n_util::GetStringF(
      IDS_EXTERNAL_PROTOCOL_APPLICATION_TO_LAUNCH, elided_command) + L"\n\n";

  message_text += l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_WARNING);

  message_box_view_ = new MessageBoxView(MessageBoxFlags::kIsConfirmMessageBox,
                                         message_text,
                                         std::wstring(),
                                         kMessageWidth);
  message_box_view_->SetCheckBoxLabel(
      l10n_util::GetString(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT));

  HWND root_hwnd;
  if (tab_contents_) {
    root_hwnd = GetAncestor(tab_contents_->GetContentNativeView(), GA_ROOT);
  } else {
    // Dialog is top level if we don't have a tab_contents associated with us.
    root_hwnd = NULL;
  }

  views::Window::CreateChromeWindow(root_hwnd, gfx::Rect(), this)->Show();
}

// static
std::wstring ExternalProtocolDialog::GetApplicationForProtocol(
    const GURL& url) {
  std::wstring url_spec = ASCIIToWide(url.possibly_invalid_spec());
  std::wstring cmd_key_path =
      ASCIIToWide(url.scheme() + "\\shell\\open\\command");
  base::win::RegKey cmd_key(HKEY_CLASSES_ROOT, cmd_key_path.c_str(), KEY_READ);
  size_t split_offset = url_spec.find(L':');
  if (split_offset == std::wstring::npos)
    return std::wstring();
  std::wstring parameters = url_spec.substr(split_offset + 1,
                                            url_spec.length() - 1);
  std::wstring application_to_launch;
  if (cmd_key.ReadValue(NULL, &application_to_launch)) {
    ReplaceSubstringsAfterOffset(&application_to_launch, 0, L"%1", parameters);
    return application_to_launch;
  } else {
    return std::wstring();
  }
}
