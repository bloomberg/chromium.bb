// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/confirm_message_box_dialog.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/message_loop.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

// static
bool ConfirmMessageBoxDialog::Run(gfx::NativeWindow parent,
                                  const std::wstring& message_text,
                                  const std::wstring& window_title) {
  ConfirmMessageBoxDialog* dialog = new ConfirmMessageBoxDialog(parent,
    message_text, window_title);
  MessageLoopForUI::current()->Run(dialog);
  return dialog->accepted();
}

ConfirmMessageBoxDialog::ConfirmMessageBoxDialog(gfx::NativeWindow parent,
    const std::wstring& message_text,
    const std::wstring& window_title)
    : message_text_(message_text),
      window_title_(window_title),
      accepted_(false),
      is_blocking_(true) {
  message_box_view_ = new MessageBoxView(MessageBoxFlags::kIsConfirmMessageBox,
                                         message_text_, window_title_);
  views::Window::CreateChromeWindow(parent, gfx::Rect(), this)->Show();
}

ConfirmMessageBoxDialog::~ConfirmMessageBoxDialog() {
}

void ConfirmMessageBoxDialog::DeleteDelegate() {
  delete this;
}

int ConfirmMessageBoxDialog::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_OK |
         MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring ConfirmMessageBoxDialog::GetWindowTitle() const {
  return window_title_;
}

std::wstring ConfirmMessageBoxDialog::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    return l10n_util::GetString(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
  }
  if (button == MessageBoxFlags::DIALOGBUTTON_CANCEL)
    return l10n_util::GetString(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
  return DialogDelegate::GetDialogButtonLabel(button);
}

views::View* ConfirmMessageBoxDialog::GetContentsView() {
  return message_box_view_;
}

bool ConfirmMessageBoxDialog::Accept() {
  is_blocking_ = false;
  accepted_ = true;
  return true;
}

bool ConfirmMessageBoxDialog::Cancel() {
  is_blocking_ = false;
  accepted_ = false;
  return true;
}

bool ConfirmMessageBoxDialog::Dispatch(const MSG& msg) {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return is_blocking_;
}
