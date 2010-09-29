// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/js_modal_dialog_views.h"

#include "app/keyboard_codes.h"
#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "chrome/browser/app_modal_dialog.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// JSModalDialogViews, public:

JSModalDialogViews::JSModalDialogViews(
    JavaScriptAppModalDialog* parent)
    : parent_(parent),
      message_box_view_(new MessageBoxView(
          parent->dialog_flags() | MessageBoxFlags::kAutoDetectAlignment,
          parent->message_text(), parent->default_prompt_text())) {
  DCHECK(message_box_view_);

  message_box_view_->AddAccelerator(
      views::Accelerator(app::VKEY_C, false, true, false));
  if (parent->display_suppress_checkbox()) {
    message_box_view_->SetCheckBoxLabel(
        l10n_util::GetString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION));
  }
}

JSModalDialogViews::~JSModalDialogViews() {
}

////////////////////////////////////////////////////////////////////////////////
// JSModalDialogViews, NativeAppModalDialog implementation:

int JSModalDialogViews::GetAppModalDialogButtons() const {
  return GetDialogButtons();
}

void JSModalDialogViews::ShowAppModalDialog() {
  window()->Show();
}

void JSModalDialogViews::ActivateAppModalDialog() {
  window()->Show();
  window()->Activate();
}

void JSModalDialogViews::CloseAppModalDialog() {
  window()->Close();
}

void JSModalDialogViews::AcceptAppModalDialog() {
  GetDialogClientView()->AcceptWindow();
}

void JSModalDialogViews::CancelAppModalDialog() {
  GetDialogClientView()->CancelWindow();
}

//////////////////////////////////////////////////////////////////////////////
// JSModalDialogViews, views::DialogDelegate implementation:

int JSModalDialogViews::GetDefaultDialogButton() const {
  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasOKButton)
    return MessageBoxFlags::DIALOGBUTTON_OK;

  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasCancelButton)
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;

  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

int JSModalDialogViews::GetDialogButtons() const {
  int dialog_buttons = 0;
  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasOKButton)
    dialog_buttons = MessageBoxFlags::DIALOGBUTTON_OK;

  if (parent_->dialog_flags() & MessageBoxFlags::kFlagHasCancelButton)
    dialog_buttons |= MessageBoxFlags::DIALOGBUTTON_CANCEL;

  return dialog_buttons;
}

std::wstring JSModalDialogViews::GetWindowTitle() const {
  return parent_->title();
}


void JSModalDialogViews::WindowClosing() {
}

void JSModalDialogViews::DeleteDelegate() {
  delete parent_;
  delete this;
}

bool JSModalDialogViews::Cancel() {
  parent_->OnCancel();
  return true;
}

bool JSModalDialogViews::Accept() {
  parent_->OnAccept(message_box_view_->GetInputText(),
                    message_box_view_->IsCheckBoxSelected());
  return true;
}

void JSModalDialogViews::OnClose() {
  parent_->OnClose();
}

std::wstring JSModalDialogViews::GetDialogButtonLabel(
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
// JSModalDialogViews, views::WindowDelegate implementation:

views::View* JSModalDialogViews::GetContentsView() {
  return message_box_view_;
}

views::View* JSModalDialogViews::GetInitiallyFocusedView() {
  if (message_box_view_->text_box())
    return message_box_view_->text_box();
  return views::DialogDelegate::GetInitiallyFocusedView();
}

////////////////////////////////////////////////////////////////////////////////
// NativeAppModalDialog, public:

// static
NativeAppModalDialog* NativeAppModalDialog::CreateNativeJavaScriptPrompt(
    JavaScriptAppModalDialog* dialog,
    gfx::NativeWindow parent_window) {
  JSModalDialogViews* d = new JSModalDialogViews(dialog);
  views::Window::CreateChromeWindow(parent_window, gfx::Rect(), d);
  return d;
}
