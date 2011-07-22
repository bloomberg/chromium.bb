// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
#pragma once

#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
class BaseScreenHandler;
class SigninScreenHandler;
}

namespace chromeos {

// A custom WebUI that defines datasource for out-of-box-experience (OOBE) UI:
// - welcome screen (setup language/keyboard/network).
// - eula screen (CrOS (+ OEM) EULA content/TPM password/crash reporting).
// - update screen.
class OobeUI : public OobeDisplay,
               public ChromeWebUI {
 public:
  explicit OobeUI(TabContents* contents);

  // OobeDisplay implementation:
  virtual void ShowScreen(WizardScreen* screen);
  virtual void HideScreen(WizardScreen* screen);
  virtual UpdateScreenActor* GetUpdateScreenActor();
  virtual NetworkScreenActor* GetNetworkScreenActor();
  virtual EulaScreenActor* GetEulaScreenActor();
  virtual EnterpriseEnrollmentScreenActor* GetEnterpriseEnrollmentScreenActor();
  virtual UserImageScreenActor* GetUserImageScreenActor();
  virtual ViewScreenDelegate* GetRegistrationScreenActor();
  virtual ViewScreenDelegate* GetHTMLPageScreenActor();

  // Collects localized strings from the owned handlers.
  void GetLocalizedStrings(base::DictionaryValue* localized_strings);

  // Initializes the handlers.
  void InitializeHandlers();

  void ShowSigninScreen();

 private:
  void AddScreenHandler(BaseScreenHandler* handler);

  // Screens actors. Note, OobeUI owns them via |handlers_|, not directly here.
  UpdateScreenActor* update_screen_actor_;
  NetworkScreenActor* network_screen_actor_;
  EulaScreenActor* eula_screen_actor_;
  EnterpriseEnrollmentScreenActor* enterprise_enrollment_screen_actor_;
  SigninScreenHandler* signin_screen_handler_;

  DISALLOW_COPY_AND_ASSIGN(OobeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_OOBE_UI_H_
