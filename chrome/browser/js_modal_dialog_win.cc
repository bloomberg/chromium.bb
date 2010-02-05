// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/js_modal_dialog.h"

#include "base/logging.h"
#include "chrome/browser/views/jsmessage_box_dialog.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "views/window/window.h"

int JavaScriptAppModalDialog::GetDialogButtons() {
  return dialog_->GetDialogButtons();
}

void JavaScriptAppModalDialog::AcceptWindow() {
  views::DialogClientView* client_view =
      dialog_->window()->GetClientView()->AsDialogClientView();
  client_view->AcceptWindow();
}

void JavaScriptAppModalDialog::CancelWindow() {
  views::DialogClientView* client_view =
      dialog_->window()->GetClientView()->AsDialogClientView();
  client_view->CancelWindow();
}

NativeDialog JavaScriptAppModalDialog::CreateNativeDialog() {
  return new JavaScriptMessageBoxDialog(this, message_text_,
      default_prompt_text_, display_suppress_checkbox_);
}

