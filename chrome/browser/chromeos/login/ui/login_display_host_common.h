// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/login/ui/login_display_host.h"

class AccountId;

namespace chromeos {

class ArcKioskController;
class DemoAppLauncher;

// LoginDisplayHostCommon contains code which is not specific to a particular UI
// implementation - the goal is to reduce code duplication between
// LoginDisplayHostViews and LoginDisplayHostWebUI.
class LoginDisplayHostCommon : public LoginDisplayHost {
 public:
  LoginDisplayHostCommon();
  ~LoginDisplayHostCommon() override;

  // LoginDisplayHost:
  AppLaunchController* GetAppLaunchController() final;
  void StartSignInScreen(const LoginScreenContext& context) final;
  void PrewarmAuthentication() final;
  void StartAppLaunch(const std::string& app_id,
                      bool diagnostic_mode,
                      bool is_auto_launch) final;
  void StartDemoAppLaunch() final;
  void StartArcKiosk(const AccountId& account_id) final;
  void CompleteLogin(const UserContext& user_context) final;
  void OnGaiaScreenReady() final;
  void SetDisplayEmail(const std::string& email) final;
  void SetDisplayAndGivenName(const std::string& display_name,
                              const std::string& given_name) final;
  void LoadWallpaper(const AccountId& account_id) final;
  void LoadSigninWallpaper() final;
  bool IsUserWhitelisted(const AccountId& account_id) final;

 protected:
  virtual void OnStartSignInScreen(const LoginScreenContext& context) = 0;
  virtual void OnStartAppLaunch() = 0;
  virtual void OnStartArcKiosk() = 0;

  // Deletes |auth_prewarmer_|.
  void OnAuthPrewarmDone();

  // Active instance of authentication prewarmer.
  std::unique_ptr<AuthPrewarmer> auth_prewarmer_;

  // App launch controller.
  std::unique_ptr<AppLaunchController> app_launch_controller_;

  // Demo app launcher.
  std::unique_ptr<DemoAppLauncher> demo_app_launcher_;

  // ARC kiosk controller.
  std::unique_ptr<ArcKioskController> arc_kiosk_controller_;

 private:
  base::WeakPtrFactory<LoginDisplayHostCommon> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostCommon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_
