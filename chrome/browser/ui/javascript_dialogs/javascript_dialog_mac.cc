// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog.h"

#include "chrome/browser/ui/blocked_content/app_modal_dialog_helper.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_cocoa.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/material_design/material_design_controller.h"

JavaScriptDialog::JavaScriptDialog(content::WebContents* parent_web_contents) {
  dialog_helper_.reset(new AppModalDialogHelper(parent_web_contents));
  parent_web_contents->GetDelegate()->ActivateContents(parent_web_contents);
}

JavaScriptDialog::~JavaScriptDialog() = default;

base::WeakPtr<JavaScriptDialog> JavaScriptDialog::Create(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const base::string16& title,
    content::JavaScriptMessageType message_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const content::JavaScriptDialogManager::DialogClosedCallback&
        dialog_callback) {
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    return JavaScriptDialogViews::Create(
        parent_web_contents, alerting_web_contents, title, message_type,
        message_text, default_prompt_text, dialog_callback);
  } else {
    return JavaScriptDialogCocoa::Create(
        parent_web_contents, alerting_web_contents, title, message_type,
        message_text, default_prompt_text, dialog_callback);
  }
}
