// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include <set>
#include <string>

#include "chrome/browser/chromeos/login/screens/app_launch_splash_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// A class that handles the WebUI hooks for the app launch splash screen.
class AppLaunchSplashScreenHandler : public BaseScreenHandler,
                                     public AppLaunchSplashScreenActor {
 public:
  AppLaunchSplashScreenHandler();
  virtual ~AppLaunchSplashScreenHandler();

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // AppLaunchSplashScreenActor implementation:
  virtual void Show(const std::string& app_id) OVERRIDE;
  virtual void PrepareToShow() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleNetworkConfig(bool visible) OVERRIDE;
  virtual void UpdateAppLaunchState(AppLaunchState state) OVERRIDE;
  virtual void SetDelegate(
      AppLaunchSplashScreenHandler::Delegate* delegate) OVERRIDE;

 private:
  void PopulateAppInfo(base::DictionaryValue* out_info);
  void SetLaunchText(const std::string& text);
  int GetProgressMessageFromState(AppLaunchState state);
  void HandleConfigureNetwork();
  void HandleCancelAppLaunch();

  AppLaunchSplashScreenHandler::Delegate* delegate_;
  bool show_on_init_;
  std::string app_id_;
  AppLaunchState state_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchSplashScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
