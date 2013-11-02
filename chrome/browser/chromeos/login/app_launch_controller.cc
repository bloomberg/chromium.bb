// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/app_launch_controller.h"

#include "apps/shell_window_registry.h"
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
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
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
AppLaunchController::CanConfigureNetworkCallback*
    AppLaunchController::can_configure_network_callback_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// AppLaunchController::AppWindowWatcher

class AppLaunchController::AppWindowWatcher
    : public apps::ShellWindowRegistry::Observer {
 public:
  explicit AppWindowWatcher(AppLaunchController* controller)
    : controller_(controller),
      window_registry_(apps::ShellWindowRegistry::Get(controller->profile_)) {
    window_registry_->AddObserver(this);
  }
  virtual ~AppWindowWatcher() {
    window_registry_->RemoveObserver(this);
  }

 private:
  // apps::ShellWindowRegistry::Observer overrides:
  virtual void OnShellWindowAdded(apps::ShellWindow* shell_window) OVERRIDE {
    if (controller_) {
      controller_->OnAppWindowCreated();
      controller_= NULL;
    }
  }
  virtual void OnShellWindowIconChanged(
      apps::ShellWindow* shell_window) OVERRIDE {}
  virtual void OnShellWindowRemoved(apps::ShellWindow* shell_window) OVERRIDE {}

  AppLaunchController* controller_;
  apps::ShellWindowRegistry* window_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowWatcher);
};

////////////////////////////////////////////////////////////////////////////////
// AppLaunchController

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
      webui_visible_(false),
      launcher_ready_(false),
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

  webui_visible_ = host_->GetWebUILoginView()->webui_visible();
  if (!webui_visible_) {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
  }
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

void AppLaunchController::SetCanConfigureNetworkCallbackForTesting(
    CanConfigureNetworkCallback* can_configure_network_callback) {
  can_configure_network_callback_ = can_configure_network_callback;
}

void AppLaunchController::OnConfigureNetwork() {
  DCHECK(profile_);
  showing_network_dialog_ = true;
  if (CanConfigureNetwork()) {
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

void AppLaunchController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE, type);
  DCHECK(!webui_visible_);
  webui_visible_ = true;
  launch_splash_start_time_ = base::TimeTicks::Now().ToInternalValue();
  if (launcher_ready_)
    OnReadyToLaunch();
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
  startup_app_launcher_->Initialize();
}

void AppLaunchController::OnProfileLoadFailed(
    KioskAppLaunchError::Error error) {
  OnLaunchFailed(error);
}

void AppLaunchController::CleanUp() {
  kiosk_profile_loader_.reset();
  startup_app_launcher_.reset();

  if (host_)
    host_->Finalize();
}

void AppLaunchController::OnNetworkWaitTimedout() {
  DCHECK(waiting_for_network_);
  LOG(WARNING) << "OnNetworkWaitTimedout... connection = "
               <<  net::NetworkChangeNotifier::GetConnectionType();
  network_wait_timedout_ = true;

  if (CanConfigureNetwork()) {
    app_launch_splash_screen_actor_->ToggleNetworkConfig(true);
  } else {
    app_launch_splash_screen_actor_->UpdateAppLaunchState(
        AppLaunchSplashScreenActor::APP_LAUNCH_STATE_NETWORK_WAIT_TIMEOUT);
  }

  if (network_timeout_callback_)
    network_timeout_callback_->Run();
}

void AppLaunchController::OnAppWindowCreated() {
  DVLOG(1) << "App window created, closing splash screen.";
  CleanUp();
}

bool AppLaunchController::CanConfigureNetwork() {
  if (can_configure_network_callback_)
    return can_configure_network_callback_->Run();

  return !UserManager::Get()->GetOwnerEmail().empty();
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

void AppLaunchController::OnReadyToLaunch() {
  launcher_ready_ = true;
  if (!webui_visible_)
    return;

  const int64 time_taken_ms = (base::TimeTicks::Now() -
      base::TimeTicks::FromInternalValue(launch_splash_start_time_)).
      InMilliseconds();

  // Enforce that we show app install splash screen for some minimum amount
  // of time.
  if (!skip_splash_wait_ && time_taken_ms < kAppInstallSplashScreenMinTimeMS) {
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&AppLaunchController::OnReadyToLaunch, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAppInstallSplashScreenMinTimeMS - time_taken_ms));
    return;
  }

  startup_app_launcher_->LaunchApp();
}

void AppLaunchController::OnLaunchSucceeded() {
  DVLOG(1) << "Kiosk launch succeeded, wait for app window.";
  app_launch_splash_screen_actor_->UpdateAppLaunchState(
      AppLaunchSplashScreenActor::APP_LAUNCH_STATE_WAITING_APP_WINDOW);

  DCHECK(!app_window_watcher_);
  app_window_watcher_.reset(new AppWindowWatcher(this));
}

void AppLaunchController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "Kiosk launch failed. Will now shut down.";
  DCHECK_NE(KioskAppLaunchError::NONE, error);

  // Saves the error and ends the session to go back to login screen.
  KioskAppLaunchError::Save(error);
  chrome::AttemptUserExit();
  CleanUp();
}

}   // namespace chromeos
