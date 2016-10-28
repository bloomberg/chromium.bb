// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_cocoa.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_views.h"

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
  if (chrome::ToolkitViewsWebUIDialogsEnabled()) {
    return JavaScriptDialogViews::Create(
        parent_web_contents, alerting_web_contents, title, message_type,
        message_text, default_prompt_text, dialog_callback);
  } else {
    return JavaScriptDialogCocoa::Create(
        parent_web_contents, alerting_web_contents, title, message_type,
        message_text, default_prompt_text, dialog_callback);
  }
}
