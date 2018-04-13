// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "content/public/browser/notification_observer.h"

class AccountId;
class ScopedKeepAlive;

namespace wm {
class ScopedDragDropDisabler;
}

namespace chromeos {

class ArcKioskController;
class DemoAppLauncher;

// LoginDisplayHostCommon contains code which is not specific to a particular UI
// implementation - the goal is to reduce code duplication between
// LoginDisplayHostMojo and LoginDisplayHostWebUI.
class LoginDisplayHostCommon : public LoginDisplayHost,
                               public content::NotificationObserver {
 public:
  LoginDisplayHostCommon();
  ~LoginDisplayHostCommon() override;

  // LoginDisplayHost:
  void BeforeSessionStart() final;
  void Finalize(base::OnceClosure completion_callback) final;
  AppLaunchController* GetAppLaunchController() final;
  void StartUserAdding(base::OnceClosure completion_callback) final;
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

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 protected:
  virtual void OnStartSignInScreen(const LoginScreenContext& context) = 0;
  virtual void OnStartAppLaunch() = 0;
  virtual void OnStartArcKiosk() = 0;
  virtual void OnBrowserCreated() = 0;
  virtual void OnStartUserAdding() = 0;
  virtual void OnFinalize() = 0;

  // Deletes |auth_prewarmer_|.
  void OnAuthPrewarmDone();

  // Marks display host for deletion.
  void ShutdownDisplayHost();

  // Active instance of authentication prewarmer.
  std::unique_ptr<AuthPrewarmer> auth_prewarmer_;

  // App launch controller.
  std::unique_ptr<AppLaunchController> app_launch_controller_;

  // Demo app launcher.
  std::unique_ptr<DemoAppLauncher> demo_app_launcher_;

  // ARC kiosk controller.
  std::unique_ptr<ArcKioskController> arc_kiosk_controller_;

  content::NotificationRegistrar registrar_;

 private:
  // True if session start is in progress.
  bool session_starting_ = false;

  // Has ShutdownDisplayHost() already been called?  Used to avoid posting our
  // own deletion to the message loop twice if the user logs out while we're
  // still in the process of cleaning up after login (http://crbug.com/134463).
  bool shutting_down_ = false;

  // Make sure chrome won't exit while we are at login/oobe screen.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Keeps a copy of the old Drag'n'Drop client, so that it would be disabled
  // during a login session and restored afterwards.
  std::unique_ptr<wm::ScopedDragDropDisabler> scoped_drag_drop_disabler_;

  // Called after host deletion.
  std::vector<base::OnceClosure> completion_callbacks_;

  base::WeakPtrFactory<LoginDisplayHostCommon> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostCommon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_COMMON_H_
