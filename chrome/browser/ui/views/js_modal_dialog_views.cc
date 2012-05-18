// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/js_modal_dialog_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// JSModalDialogViews, public:

JSModalDialogViews::JSModalDialogViews(JavaScriptAppModalDialog* parent)
    : parent_(parent) {
  int options = views::MessageBoxView::DETECT_DIRECTIONALITY;
  if (parent->javascript_message_type() == ui::JAVASCRIPT_MESSAGE_TYPE_PROMPT)
    options |= views::MessageBoxView::HAS_PROMPT_FIELD;

  views::MessageBoxView::InitParams params(parent->message_text());
  params.options = options;
  params.default_prompt = parent->default_prompt_text();
  message_box_view_ = new views::MessageBoxView(params);
  DCHECK(message_box_view_);

  message_box_view_->AddAccelerator(
      ui::Accelerator(ui::VKEY_C, ui::EF_CONTROL_DOWN));
  if (parent->display_suppress_checkbox()) {
    message_box_view_->SetCheckBoxLabel(
        l10n_util::GetStringUTF16(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION));
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
  GetWidget()->Show();
}

void JSModalDialogViews::ActivateAppModalDialog() {
  GetWidget()->Show();
  GetWidget()->Activate();
}

void JSModalDialogViews::CloseAppModalDialog() {
  GetWidget()->Close();
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
  return ui::DIALOG_BUTTON_OK;
}

int JSModalDialogViews::GetDialogButtons() const {
  if (parent_->javascript_message_type() == ui::JAVASCRIPT_MESSAGE_TYPE_ALERT)
    return ui::DIALOG_BUTTON_OK;

  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

string16 JSModalDialogViews::GetWindowTitle() const {
  return parent_->title();
}

void JSModalDialogViews::WindowClosing() {
}

void JSModalDialogViews::DeleteDelegate() {
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

views::Widget* JSModalDialogViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* JSModalDialogViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

string16 JSModalDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (parent_->is_before_unload_dialog()) {
    if (button == ui::DIALOG_BUTTON_OK) {
      return l10n_util::GetStringUTF16(
          parent_->is_reload() ?
          IDS_BEFORERELOAD_MESSAGEBOX_OK_BUTTON_LABEL :
          IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
    } else if (button == ui::DIALOG_BUTTON_CANCEL) {
      return l10n_util::GetStringUTF16(
          parent_->is_reload() ?
          IDS_BEFORERELOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL :
          IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
    }
  }
  return DialogDelegate::GetDialogButtonLabel(button);
}

///////////////////////////////////////////////////////////////////////////////
// JSModalDialogViews, views::WidgetDelegate implementation:

ui::ModalType JSModalDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
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
  views::Widget::CreateWindowWithParent(d, parent_window);
  return d;
}
