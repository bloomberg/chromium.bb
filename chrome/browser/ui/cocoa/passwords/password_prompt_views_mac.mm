// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#include "chrome/browser/ui/cocoa/passwords/password_prompt_view_bridge.h"
#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"
#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"

AccountChooserPrompt* CreateAccountChooserPromptView(
    PasswordDialogController* controller,
    content::WebContents* web_contents) {
  if (chrome::ShowAllDialogsWithViewsToolkit())
    return new AccountChooserDialogView(controller, web_contents);
  return new PasswordPromptViewBridge(controller, web_contents);
}

AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    PasswordDialogController* controller,
    content::WebContents* web_contents) {
  if (chrome::ShowAllDialogsWithViewsToolkit())
    return new AutoSigninFirstRunDialogView(controller, web_contents);
  return new PasswordPromptViewBridge(controller, web_contents);
}
