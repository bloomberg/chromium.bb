// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/app_launch_controller.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Application install splash screen minimum show time in milliseconds.
const int kAppInstallSplashScreenMinTimeMS = 3000;

}  // namespace

// static
bool AppLaunchController::skip_splash_wait_ = false;

AppLaunchController::AppLaunchController(const std::string& app_id,
                                         LoginDisplayHost* host,
                                         OobeDisplay* oobe_display)
    : profile_(NULL),
      app_id_(app_id),
      host_(host),
      oobe_display_(oobe_display),
      app_launch_splash_screen_actor_(
          oobe_display_->GetAppLaunchSplashScreenActor()),
      launch_splash_start_time_(0) {
}

AppLaunchController::~AppLaunchController() {
}

void AppLaunchController::StartAppLaunch() {
  DVLOG(1) << "Starting kiosk mode...";
  launch_splash_start_time_ = base::TimeTicks::Now().ToInternalValue();

  // TODO(tengs): Add a loading profile app launch state.
  app_launch_splash_screen_actor_->SetDelegate(this);
  app_launch_splash_screen_actor_->Show(app_id_);

  // KioskProfileLoader manages its own lifetime.
  kiosk_profile_loader_.reset(
      new KioskProfileLoader(KioskAppManager::Get(), app_id_, this));
  kiosk_profile_loader_->Start();
}

// static
void AppLaunchController::SkipSplashWaitForTesting() {
  skip_splash_wait_ = true;
}

void AppLaunchController::OnConfigureNetwork() {
  // TODO(tengs): Implement network configuration in app launch.
}

void AppLaunchController::OnCancelAppLaunch() {
  if (KioskAppManager::Get()->GetDisableBailoutShortcut())
    return;

  OnLaunchFailed(KioskAppLaunchError::USER_CANCEL);
}

void AppLaunchController::OnProfileLoaded(Profile* profile) {
  DVLOG(1) << "Profile loaded... Starting app launch.";
  profile_ = profile;

  kiosk_profile_loader_.reset();
  startup_app_launcher_.reset(new StartupAppLauncher(profile_, app_id_));
  startup_app_launcher_->AddObserver(this);
  startup_app_launcher_->Start();
}

void AppLaunchController::OnProfileLoadFailed(
    KioskAppLaunchError::Error error) {
  OnLaunchFailed(error);
}

void AppLaunchController::Cleanup() {
  kiosk_profile_loader_.reset();
  startup_app_launcher_.reset();

  if (host_)
    host_->Finalize();
}

void AppLaunchController::OnLoadingOAuthFile() {
  app_launch_splash_screen_actor_->UpdateAppLaunchState(
      AppLaunchSplashScreenActor::APP_LAUNCH_STATE_LOADING_AUTH_FILE);
}

void AppLaunchController::OnInitializingTokenService() {
  app_launch_splash_screen_actor_->UpdateAppLaunchState(
      AppLaunchSplashScreenActor::APP_LAUNCH_STATE_LOADING_TOKEN_SERVICE);
}

void AppLaunchController::OnInitializingNetwork() {
  app_launch_splash_screen_actor_->UpdateAppLaunchState(
      AppLaunchSplashScreenActor::APP_LAUNCH_STATE_PREPARING_NETWORK);
}

void AppLaunchController::OnNetworkWaitTimedout() {
}

void AppLaunchController::OnInstallingApp() {
  app_launch_splash_screen_actor_->UpdateAppLaunchState(
      AppLaunchSplashScreenActor::APP_LAUNCH_STATE_INSTALLING_APPLICATION);
}

void AppLaunchController::OnLaunchSucceeded() {
  const int64 time_taken_ms = (base::TimeTicks::Now() -
      base::TimeTicks::FromInternalValue(launch_splash_start_time_)).
      InMilliseconds();

  // Enforce that we show app install splash screen for some minimum amount
  // of time.
  if (!skip_splash_wait_ && time_taken_ms < kAppInstallSplashScreenMinTimeMS) {
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AppLaunchController::OnLaunchSucceeded, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAppInstallSplashScreenMinTimeMS - time_taken_ms));
    return;
  }

  DVLOG(1) << "Kiosk launch succeeded!";
  Cleanup();
}

void AppLaunchController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "Kiosk launch failed. Will now shut down.";
  DCHECK_NE(KioskAppLaunchError::NONE, error);

  // Saves the error and ends the session to go back to login screen.
  KioskAppLaunchError::Save(error);
  chrome::AttemptUserExit();
  Cleanup();
}

}   // namespace chromeos
