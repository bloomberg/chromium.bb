// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include <set>
#include <string>

#include "chrome/browser/chromeos/login/screens/app_launch_splash_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"

namespace chromeos {

class ErrorScreenActor;

// A class that handles the WebUI hooks for the app launch splash screen.
class AppLaunchSplashScreenHandler
    : public BaseScreenHandler,
      public AppLaunchSplashScreenActor,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  AppLaunchSplashScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreenActor* error_screen_actor);
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
  virtual void ShowNetworkConfigureUI() OVERRIDE;
  virtual bool IsNetworkReady() OVERRIDE;

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  virtual void OnNetworkReady() OVERRIDE;
  virtual void UpdateState(ErrorScreenActor::ErrorReason reason) OVERRIDE;

 private:
  void PopulateAppInfo(base::DictionaryValue* out_info);
  void SetLaunchText(const std::string& text);
  int GetProgressMessageFromState(AppLaunchState state);
  void HandleConfigureNetwork();
  void HandleCancelAppLaunch();
  void HandleContinueAppLaunch();
  void HandleNetworkConfigRequested();

  AppLaunchSplashScreenHandler::Delegate* delegate_;
  bool show_on_init_;
  std::string app_id_;
  AppLaunchState state_;

  scoped_refptr<NetworkStateInformer> network_state_informer_;
  ErrorScreenActor* error_screen_actor_;
  // True if we are online.
  bool online_state_;
  // True if we have network config screen was already shown before.
  bool network_config_done_;
  // True if we have manually requested network config screen.
  bool network_config_requested_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchSplashScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
