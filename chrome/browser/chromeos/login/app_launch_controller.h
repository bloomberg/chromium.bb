// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_profile_loader.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/login/app_launch_signin_screen.h"
#include "chrome/browser/chromeos/login/screens/app_launch_splash_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"

class Profile;

namespace chromeos {

class LoginDisplayHost;
class OobeDisplay;

// Controller for the kiosk app launch process, responsible for
// coordinating loading the kiosk profile, launching the app, and
// updating the splash screen UI.
class AppLaunchController
    : public base::SupportsWeakPtr<AppLaunchController>,
      public AppLaunchSplashScreenActor::Delegate,
      public KioskProfileLoader::Delegate,
      public StartupAppLauncher::Observer,
      public AppLaunchSigninScreen::Delegate {
 public:
  AppLaunchController(const std::string& app_id,
                      LoginDisplayHost* host,
                      OobeDisplay* oobe_display);

  virtual ~AppLaunchController();

  void StartAppLaunch();

  static void SkipSplashWaitForTesting();

 private:
  void Cleanup();
  void OnNetworkWaitTimedout();

  // KioskProfileLoader::Delegate overrides:
  virtual void OnProfileLoaded(Profile* profile) OVERRIDE;
  virtual void OnProfileLoadFailed(KioskAppLaunchError::Error error) OVERRIDE;

  // AppLaunchSplashScreenActor::Delegate overrides:
  virtual void OnConfigureNetwork() OVERRIDE;
  virtual void OnCancelAppLaunch() OVERRIDE;

  // StartupAppLauncher::Observer overrides:
  virtual void OnLoadingOAuthFile() OVERRIDE;
  virtual void OnInitializingTokenService() OVERRIDE;
  virtual void OnInitializingNetwork() OVERRIDE;
  virtual void OnInstallingApp() OVERRIDE;
  virtual void OnLaunchSucceeded() OVERRIDE;
  virtual void OnLaunchFailed(KioskAppLaunchError::Error error) OVERRIDE;

  // AppLaunchSigninScreen::Delegate overrides:
  virtual void OnOwnerSigninSuccess() OVERRIDE;

  Profile* profile_;
  const std::string app_id_;
  LoginDisplayHost* host_;
  OobeDisplay* oobe_display_;
  AppLaunchSplashScreenActor* app_launch_splash_screen_actor_;
  ErrorScreenActor* error_screen_actor_;
  scoped_ptr<KioskProfileLoader> kiosk_profile_loader_;
  scoped_ptr<StartupAppLauncher> startup_app_launcher_;
  scoped_ptr<AppLaunchSigninScreen> signin_screen_;

  base::OneShotTimer<AppLaunchController> network_wait_timer_;
  bool waiting_for_network_;
  bool showing_network_dialog_;
  int64 launch_splash_start_time_;

  static bool skip_splash_wait_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_APP_LAUNCH_CONTROLLER_H_
