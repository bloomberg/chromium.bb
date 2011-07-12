// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/chromeos/login/login_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/browser/webui/web_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

// Signin screen handler.
class SigninScreenHandler : public OobeMessageHandler,
                            public BaseLoginUIHandler {
 public:
  SigninScreenHandler();

  // Shows the sign in screen.
  void Show();

 private:
  // OobeMessageHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // BaseLoginUIHandler implementation.
  virtual void ClearAndEnablePassword() OVERRIDE;
  virtual void ShowError(const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;

  // Handles authenticate user request from javascript.
  void HandleAuthenticateUser(const base::ListValue* args);

  // A delegate that glues this handler with backend LoginDisplay.
  LoginUIHandlerDelegate* delegate_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(SigninScreenHandler);
};

}  // namespae chromeoc

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
