// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/app_launch_controller.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

namespace chromeos {

namespace {

// Application install splash screen minimum show time in milliseconds.
const int kAppInstallSplashScreenMinTimeMS = 3000;

}  // namespace

// static
bool AppLaunchController::skip_splash_wait_ = false;
int AppLaunchController::network_wait_time_ = 10;
base::Closure* AppLaunchController::network_timeout_callback_ = NULL;
UserManager* AppLaunchController::test_user_manager_ = NULL;

AppLaunchController::AppLaunchController(const std::string& app_id,
                                         LoginDisplayHost* host,
                                         OobeDisplay* oobe_display)
    : profile_(NULL),
      app_id_(app_id),
      host_(host),
      oobe_display_(oobe_display),
      app_launch_splash_screen_actor_(
          oobe_display_->GetAppLaunchSplashScreenActor()),
      error_screen_actor_(oobe_display_->GetErrorScreenActor()),
      waiting_for_network_(false),
      network_wait_timedout_(false),
      showing_network_dialog_(false),
      launch_splash_start_time_(0) {
  signin_screen_.reset(new AppLaunchSigninScreen(
     static_cast<OobeUI*>(oobe_display_), this));
}

AppLaunchController::~AppLaunchController() {
  app_launch_splash_screen_actor_->SetDelegate(NULL);
}

void AppLaunchController::StartAppLaunch() {
  DVLOG(1) << "Starting kiosk mode...";
  launch_splash_start_time_ = base::TimeTicks::Now().ToInternalValue();

  // TODO(tengs): Add a loading profile app launch state.
  app_launch_splash_screen_actor_->SetDelegate(this);
  app_launch_splash_screen_actor_->Show(app_id_);

  kiosk_profile_loader_.reset(
      new KioskProfileLoader(KioskAppManager::Get(), app_id_, this));
  kiosk_profile_loader_->Start();
}

void AppLaunchController::SkipSplashWaitForTesting() {
  skip_splash_wait_ = true;
}

void AppLaunchController::SetNetworkWaitForTesting(int wait_time_secs) {
  network_wait_time_ = wait_time_secs;
}

void AppLaunchController::SetNetworkTimeoutCallbackForTesting(
   base::Closure* callback) {
  network_timeout_callback_ = callback;
}

void AppLaunchController::SetUserManagerForTesting(UserManager* user_manager) {
  test_user_manager_ = user_manager;
  AppLaunchSigninScreen::SetUserManagerForTesting(user_manager);
}

void AppLaunchController::OnConfigureNetwork() {
  DCHECK(profile_);
  showing_network_dialog_ = true;
  const std::string& owner_email = GetUserManager()->GetOwnerEmail();
  if (!owner_email.empty()) {
    signin_screen_->Show();
  } else {
    // If kiosk mode was configured through enterprise policy, we may
    // not have an owner user.
    // TODO(tengs): We need to figure out the appropriate security meausres
    // for this case.
    NOTREACHED();
  }
}

void AppLaunchController::OnOwnerSigninSuccess() {
  error_screen_actor_->SetErrorState(
      ErrorScreen::ERROR_STATE_OFFLINE, std::string());
  error_screen_actor_->SetUIState(ErrorScreen::UI_STATE_KIOSK_MODE);

  error_screen_actor_->Show(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH, NULL);

  signin_screen_.reset();
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

  // Show the network configration dialog if network is not initialized
  // after a brief wait time.
  waiting_for_network_ = true;
  network_wait_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(network_wait_time_),
      this, &AppLaunchController::OnNetworkWaitTimedout);
}

void AppLaunchController::OnNetworkWaitTimedout() {
  DCHECK(waiting_for_network_);
  LOG(WARNING) << "OnNetworkWaitTimedout... connection = "
               <<  net::NetworkChangeNotifier::GetConnectionType();
  network_wait_timedout_ = true;
  app_launch_splash_screen_actor_->ToggleNetworkConfig(true);
  if (network_timeout_callback_)
    network_timeout_callback_->Run();
}

void AppLaunchController::OnInstallingApp() {
  app_launch_splash_screen_actor_->UpdateAppLaunchState(
      AppLaunchSplashScreenActor::APP_LAUNCH_STATE_INSTALLING_APPLICATION);

  network_wait_timer_.Stop();
  app_launch_splash_screen_actor_->ToggleNetworkConfig(false);

  // We have connectivity at this point, so we can skip the network
  // configuration dialog if it is being shown.
  if (showing_network_dialog_) {
    app_launch_splash_screen_actor_->Show(app_id_);
    showing_network_dialog_ = false;
    launch_splash_start_time_ = base::TimeTicks::Now().ToInternalValue();
  }
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

UserManager* AppLaunchController::GetUserManager() {
  return test_user_manager_ ? test_user_manager_ : UserManager::Get();
}

}   // namespace chromeos
