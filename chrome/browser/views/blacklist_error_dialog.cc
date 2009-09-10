// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/blacklist_error_dialog.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/logging.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

// static
bool BlacklistErrorDialog::RunBlacklistErrorDialog() {
  // When the window closes, it will delete itself.
  BlacklistErrorDialog* dlg = new BlacklistErrorDialog;
  MessageLoopForUI::current()->Run(dlg);
  return dlg->accepted();
}

BlacklistErrorDialog::BlacklistErrorDialog()
    : is_blocking_(true), accepted_(false) {
  const int kDialogWidth = 400;
  std::wstring message = l10n_util::GetString(IDS_BLACKLIST_ERROR_LOADING_TEXT);
  box_view_ = new MessageBoxView(MessageBoxFlags::kIsConfirmMessageBox,
                                 message.c_str(), std::wstring(), kDialogWidth);

  views::Window::CreateChromeWindow(NULL, gfx::Rect(), this)->Show();
}

BlacklistErrorDialog::~BlacklistErrorDialog() {
}

std::wstring BlacklistErrorDialog::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
    case MessageBoxFlags::DIALOGBUTTON_OK:
      return l10n_util::GetString(IDS_BLACKLIST_ERROR_LOADING_CONTINUE);
    case MessageBoxFlags::DIALOGBUTTON_CANCEL:
      return l10n_util::GetString(IDS_BLACKLIST_ERROR_LOADING_EXIT);
  }
  return std::wstring();
}

std::wstring BlacklistErrorDialog::GetWindowTitle() const {
  return l10n_util::GetString(IDS_BLACKLIST_ERROR_LOADING_TITLE);
}

void BlacklistErrorDialog::DeleteDelegate() {
  delete this;
}

bool BlacklistErrorDialog::Accept() {
  is_blocking_ = false;
  accepted_ = true;
  return true;
}

bool BlacklistErrorDialog::Cancel() {
  is_blocking_ = false;
  accepted_ = false;
  return true;
}

views::View* BlacklistErrorDialog::GetContentsView() {
  return box_view_;
}

bool BlacklistErrorDialog::Dispatch(const MSG& msg) {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return is_blocking_;
}
