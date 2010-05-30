// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/jsmessage_box_dialog.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

JavaScriptMessageBoxDialog::JavaScriptMessageBoxDialog(
    JavaScriptAppModalDialog* parent,
    const std::wstring& message_text,
    const std::wstring& default_prompt_text,
    bool display_suppress_checkbox)
    : parent_(parent),
      message_box_view_(new MessageBoxView(
          parent->dialog_flags() | MessageBoxFlags::kAutoDetectAlignment,
          message_text, default_prompt_text)) {
  DCHECK(message_box_view_);

  message_box_view_->AddAccelerator(
      views::Accelerator(base::VKEY_C, false, true, false));
  if (display_suppress_checkbox) {
    message_box_view_->SetCheckBoxLabel(
        l10n_util::GetString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION));
  }
}

JavaScriptMessageBoxDialog::~JavaScriptMessageBoxDialog() {
}

gfx::NativeWindow JavaScriptMessageBoxDialog::GetDialogRootWindow() {
  return client()->GetMessageBoxRootWindow();
}

//////////////////////////////////////////////////////////////////////////////
// JavaScriptMessageBoxDialog, views::DialogDelegate implementation:

int JavaScriptMessageBoxDialog::GetDefaultDialogButton() const {
  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasOKButton)
    return MessageBoxFlags::DIALOGBUTTON_OK;

  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasCancelButton)
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;

  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

int JavaScriptMessageBoxDialog::GetDialogButtons() const {
  int dialog_buttons = 0;
  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasOKButton)
    dialog_buttons = MessageBoxFlags::DIALOGBUTTON_OK;

  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasCancelButton)
    dialog_buttons |= MessageBoxFlags::DIALOGBUTTON_CANCEL;

  return dialog_buttons;
}

std::wstring JavaScriptMessageBoxDialog::GetWindowTitle() const {
  return parent_->title();
}


void JavaScriptMessageBoxDialog::WindowClosing() {
}

void JavaScriptMessageBoxDialog::DeleteDelegate() {
  delete parent_;
  delete this;
}

bool JavaScriptMessageBoxDialog::Cancel() {
  parent_->OnCancel();
  return true;
}

bool JavaScriptMessageBoxDialog::Accept() {
  parent_->OnAccept(message_box_view_->GetInputText(),
                    message_box_view_->IsCheckBoxSelected());
  return true;
}

void JavaScriptMessageBoxDialog::OnClose() {
  parent_->OnClose();
}

std::wstring JavaScriptMessageBoxDialog::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (parent_->is_before_unload_dialog()) {
    if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
      return l10n_util::GetString(IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
    } else if (button == MessageBoxFlags::DIALOGBUTTON_CANCEL) {
      return l10n_util::GetString(
          IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
    }
  }
  return DialogDelegate::GetDialogButtonLabel(button);
}

///////////////////////////////////////////////////////////////////////////////
// JavaScriptMessageBoxDialog, views::WindowDelegate implementation:

views::View* JavaScriptMessageBoxDialog::GetContentsView() {
  return message_box_view_;
}

views::View* JavaScriptMessageBoxDialog::GetInitiallyFocusedView() {
  if (message_box_view_->text_box())
    return message_box_view_->text_box();
  return views::DialogDelegate::GetInitiallyFocusedView();
}
