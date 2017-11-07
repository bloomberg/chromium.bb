// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHANGE_PASSWORD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHANGE_PASSWORD_HANDLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace safe_browsing {
class ChromePasswordProtectionService;
}

namespace settings {

// Chrome "Change Password" settings page UI handler.
class ChangePasswordHandler
    : public SettingsPageUIHandler,
      public safe_browsing::ChromePasswordProtectionService::Observer {
 public:
  explicit ChangePasswordHandler(Profile* profile);
  ~ChangePasswordHandler() override;

  // settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // safe_browsing::ChromePasswordProtectionService::Observer:
  void OnStartingGaiaPasswordChange() override {}
  void OnGaiaPasswordChanged() override;
  void OnMarkingSiteAsLegitimate(const GURL& url) override;
  void OnGaiaPasswordReuseWarningShown() override;
  void InvokeActionForTesting(
      safe_browsing::ChromePasswordProtectionService::WarningAction action)
      override;
  safe_browsing::ChromePasswordProtectionService::WarningUIType
  GetObserverType() override;

 private:
  void HandleInitialize(const base::ListValue* args);

  void HandleChangePasswordPageShown(const base::ListValue* args);

  void HandleChangePassword(const base::ListValue* args);

  Profile* profile_;

  PrefChangeRegistrar pref_registrar_;

  safe_browsing::ChromePasswordProtectionService* service_;

  // Listen to ChromePasswordProtectionService notification.
  ScopedObserver<safe_browsing::ChromePasswordProtectionService,
                 safe_browsing::ChromePasswordProtectionService::Observer>
      password_protection_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChangePasswordHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHANGE_PASSWORD_HANDLER_H_
