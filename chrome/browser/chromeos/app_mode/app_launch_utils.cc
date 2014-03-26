// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_launch_utils.h"

#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/lifetime/application_lifetime.h"

namespace chromeos {

// A simple manager for the app launch that starts the launch
// and deletes itself when the launch finishes. On launch failure,
// it exits the browser process.
class AppLaunchManager : public StartupAppLauncher::Delegate {
 public:
  AppLaunchManager(Profile* profile, const std::string& app_id)
      : startup_app_launcher_(
            new StartupAppLauncher(profile,
                                   app_id,
                                   false /* diagnostic_mode */,
                                   this)) {}

  void Start() {
    startup_app_launcher_->Initialize();
  }

 private:
  virtual ~AppLaunchManager() {}

  void Cleanup() { delete this; }

  // StartupAppLauncher::Delegate overrides:
  virtual void InitializeNetwork() OVERRIDE {
    // This is on crash-restart path and assumes network is online.
    // TODO(xiyuan): Remove the crash-restart path for kiosk or add proper
    // network configure handling.
    startup_app_launcher_->ContinueWithNetworkReady();
  }
  virtual bool IsNetworkReady() OVERRIDE {
    // See comments above. Network is assumed to be online here.
    return true;
  }
  virtual void OnLoadingOAuthFile() OVERRIDE {}
  virtual void OnInitializingTokenService() OVERRIDE {}
  virtual void OnInstallingApp() OVERRIDE {}
  virtual void OnReadyToLaunch() OVERRIDE {
    startup_app_launcher_->LaunchApp();
  }
  virtual void OnLaunchSucceeded() OVERRIDE { Cleanup(); }
  virtual void OnLaunchFailed(KioskAppLaunchError::Error error) OVERRIDE {
    KioskAppLaunchError::Save(error);
    chrome::AttemptUserExit();
    Cleanup();
  }
  virtual bool IsShowingNetworkConfigScreen() OVERRIDE { return false; }

  scoped_ptr<StartupAppLauncher> startup_app_launcher_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchManager);
};

void LaunchAppOrDie(Profile* profile, const std::string& app_id) {
  // AppLaunchManager manages its own lifetime.
  (new AppLaunchManager(profile, app_id))->Start();
}

}  // namespace chromeos
