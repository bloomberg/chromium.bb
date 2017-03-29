// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_error_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

SigninErrorHandler::SigninErrorHandler(bool is_system_profile)
    : is_system_profile_(is_system_profile) {}

void SigninErrorHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "confirm",
      base::Bind(&SigninErrorHandler::HandleConfirm, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "switchToExistingProfile",
      base::Bind(&SigninErrorHandler::HandleSwitchToExistingProfile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "learnMore",
      base::Bind(&SigninErrorHandler::HandleLearnMore, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializedWithSize",
      base::Bind(&SigninErrorHandler::HandleInitializedWithSize,
                 base::Unretained(this)));
}

void SigninErrorHandler::HandleSwitchToExistingProfile(
    const base::ListValue* args) {
  if (duplicate_profile_path_.empty())
    return;

  // CloseDialog will eventually destroy this object, so nothing should access
  // its members after this call. However, closing the dialog may steal focus
  // back to the original window, so make a copy of the path to switch to and
  // perform the switch after the dialog is closed.
  base::FilePath path_switching_to = duplicate_profile_path_;
  CloseDialog();

  // Switch to the existing duplicate profile. Do not create a new window when
  // any existing ones can be reused.
  profiles::SwitchToProfile(path_switching_to, false,
                            ProfileManager::CreateCallback(),
                            ProfileMetrics::SWITCH_PROFILE_DUPLICATE);
}

void SigninErrorHandler::HandleConfirm(const base::ListValue* args) {
  CloseDialog();
}

void SigninErrorHandler::HandleLearnMore(const base::ListValue* args) {
  Browser* browser = signin::GetDesktopBrowser(web_ui());
  DCHECK(browser);
  browser->CloseModalSigninWindow();
  signin_ui_util::ShowSigninErrorLearnMorePage(browser->profile());
}

void SigninErrorHandler::HandleInitializedWithSize(
    const base::ListValue* args) {
  AllowJavascript();
  if (duplicate_profile_path_.empty())
    CallJavascriptFunction("signin.error.removeSwitchButton");

  signin::SetInitializedModalHeight(web_ui(), args);

  // After the dialog is shown, some platforms might have an element focused.
  // To be consistent, clear the focused element on all platforms.
  // TODO(anthonyvd): Figure out why this is needed on Mac and not other
  // platforms and if there's a way to start unfocused while avoiding this
  // workaround.
  CallJavascriptFunction("signin.error.clearFocus");
}

void SigninErrorHandler::CloseDialog() {
  if (is_system_profile_) {
    // Avoid closing the user manager window when the error message is displayed
    // without browser window.
    UserManagerProfileDialog::HideDialog();
  } else {
    Browser* browser = signin::GetDesktopBrowser(web_ui());
    DCHECK(browser);
    browser->CloseModalSigninWindow();
  }
}
