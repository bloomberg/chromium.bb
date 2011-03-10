// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/js_modal_dialog_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// JSModalDialogViews, public:

JSModalDialogViews::JSModalDialogViews(
    JavaScriptAppModalDialog* parent)
    : parent_(parent),
      message_box_view_(new MessageBoxView(
          parent->dialog_flags() | ui::MessageBoxFlags::kAutoDetectAlignment,
          parent->message_text(), parent->default_prompt_text())) {
  DCHECK(message_box_view_);

  message_box_view_->AddAccelerator(
      views::Accelerator(ui::VKEY_C, false, true, false));
  if (parent->display_suppress_checkbox()) {
    message_box_view_->SetCheckBoxLabel(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION)));
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
  window()->CloseWindow();
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
  if (parent_->dialog_flags() & ui::MessageBoxFlags::kFlagHasOKButton)
    return ui::MessageBoxFlags::DIALOGBUTTON_OK;

  if (parent_->dialog_flags() & ui::MessageBoxFlags::kFlagHasCancelButton)
    return ui::MessageBoxFlags::DIALOGBUTTON_CANCEL;

  return ui::MessageBoxFlags::DIALOGBUTTON_NONE;
}

int JSModalDialogViews::GetDialogButtons() const {
  int dialog_buttons = 0;
  if (parent_->dialog_flags() & ui::MessageBoxFlags::kFlagHasOKButton)
    dialog_buttons = ui::MessageBoxFlags::DIALOGBUTTON_OK;

  if (parent_->dialog_flags() & ui::MessageBoxFlags::kFlagHasCancelButton)
    dialog_buttons |= ui::MessageBoxFlags::DIALOGBUTTON_CANCEL;

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
  parent_->OnCancel(message_box_view_->IsCheckBoxSelected());
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
    ui::MessageBoxFlags::DialogButton button) const {
  if (parent_->is_before_unload_dialog()) {
    if (button == ui::MessageBoxFlags::DIALOGBUTTON_OK) {
      return UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL));
    } else if (button == ui::MessageBoxFlags::DIALOGBUTTON_CANCEL) {
      return UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL));
    }
  }
  return DialogDelegate::GetDialogButtonLabel(button);
}

///////////////////////////////////////////////////////////////////////////////
// JSModalDialogViews, views::WindowDelegate implementation:

bool JSModalDialogViews::IsModal() const {
  return true;
}

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

  browser::CreateViewsWindow(parent_window, gfx::Rect(), d);
  return d;
}
