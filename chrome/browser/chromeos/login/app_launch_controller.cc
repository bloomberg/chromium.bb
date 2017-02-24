// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/app_launch_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/login/enterprise_user_session_metrics.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/features/feature_session_type.h"
#include "net/base/network_change_notifier.h"

namespace chromeos {

namespace {

// Enum types for Kiosk.LaunchType UMA so don't change its values.
// KioskLaunchType in histogram.xml must be updated when making changes here.
enum KioskLaunchType {
  KIOSK_LAUNCH_ENTERPRISE_AUTO_LAUNCH = 0,
  KIOKS_LAUNCH_ENTERPRISE_MANUAL_LAUNCH = 1,
  KIOSK_LAUNCH_CONSUMER_AUTO_LAUNCH = 2,
  KIOSK_LAUNCH_CONSUMER_MANUAL_LAUNCH = 3,

  KIOSK_LAUNCH_TYPE_COUNT  // This must be the last entry.
};

// Application install splash screen minimum show time in milliseconds.
const int kAppInstallSplashScreenMinTimeMS = 3000;

bool IsEnterpriseManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->IsEnterpriseManaged();
}

void RecordKioskLaunchUMA(bool is_auto_launch) {
  const KioskLaunchType launch_type =
      IsEnterpriseManaged()
          ? (is_auto_launch ? KIOSK_LAUNCH_ENTERPRISE_AUTO_LAUNCH
                            : KIOKS_LAUNCH_ENTERPRISE_MANUAL_LAUNCH)
          : (is_auto_launch ? KIOSK_LAUNCH_CONSUMER_AUTO_LAUNCH
                            : KIOSK_LAUNCH_CONSUMER_MANUAL_LAUNCH);

  UMA_HISTOGRAM_ENUMERATION("Kiosk.LaunchType", launch_type,
                            KIOSK_LAUNCH_TYPE_COUNT);

  if (IsEnterpriseManaged()) {
    enterprise_user_session_metrics::RecordSignInEvent(
        is_auto_launch
            ? enterprise_user_session_metrics::SignInEventType::AUTOMATIC_KIOSK
            : enterprise_user_session_metrics::SignInEventType::MANUAL_KIOSK);
  }
}

}  // namespace

// static
bool AppLaunchController::skip_splash_wait_ = false;
int AppLaunchController::network_wait_time_ = 10;
base::Closure* AppLaunchController::network_timeout_callback_ = NULL;
AppLaunchController::ReturnBoolCallback*
    AppLaunchController::can_configure_network_callback_ = NULL;
AppLaunchController::ReturnBoolCallback*
    AppLaunchController::need_owner_auth_to_configure_network_callback_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// AppLaunchController::AppWindowWatcher

class AppLaunchController::AppWindowWatcher
    : public extensions::AppWindowRegistry::Observer {
 public:
  explicit AppWindowWatcher(AppLaunchController* controller,
                            const std::string& app_id)
      : controller_(controller),
        app_id_(app_id),
        window_registry_(
            extensions::AppWindowRegistry::Get(controller->profile_)),
        weak_factory_(this) {
    if (!window_registry_->GetAppWindowsForApp(app_id).empty()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&AppWindowWatcher::NotifyAppWindowCreated,
                                weak_factory_.GetWeakPtr()));
      return;
    } else {
      window_registry_->AddObserver(this);
    }
  }
  ~AppWindowWatcher() override { window_registry_->RemoveObserver(this); }

 private:
  // extensions::AppWindowRegistry::Observer overrides:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    if (app_window->extension_id() == app_id_) {
      window_registry_->RemoveObserver(this);
      NotifyAppWindowCreated();
    }
  }

  void NotifyAppWindowCreated() {
    controller_->OnAppWindowCreated();
  }

  AppLaunchController* controller_;
  std::string app_id_;
  extensions::AppWindowRegistry* window_registry_;
  base::WeakPtrFactory<AppWindowWatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowWatcher);
};

////////////////////////////////////////////////////////////////////////////////
// AppLaunchController

AppLaunchController::AppLaunchController(const std::string& app_id,
                                         bool diagnostic_mode,
                                         LoginDisplayHost* host,
                                         OobeUI* oobe_ui)
    : app_id_(app_id),
      diagnostic_mode_(diagnostic_mode),
      host_(host),
      oobe_ui_(oobe_ui),
      app_launch_splash_screen_view_(oobe_ui_->GetAppLaunchSplashScreenView()) {
}

AppLaunchController::~AppLaunchController() {
  app_launch_splash_screen_view_->SetDelegate(NULL);
}

void AppLaunchController::StartAppLaunch(bool is_auto_launch) {
  DVLOG(1) << "Starting kiosk mode...";

  RecordKioskLaunchUMA(is_auto_launch);

  // Ensure WebUILoginView is enabled so that bailout shortcut key works.
  host_->GetWebUILoginView()->SetUIEnabled(true);

  webui_visible_ = host_->GetWebUILoginView()->webui_visible();
  if (!webui_visible_) {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
  }
  launch_splash_start_time_ = base::TimeTicks::Now().ToInternalValue();

  // TODO(tengs): Add a loading profile app launch state.
  app_launch_splash_screen_view_->SetDelegate(this);
  app_launch_splash_screen_view_->Show(app_id_);

  KioskAppManager::App app;
  CHECK(KioskAppManager::Get());
  CHECK(KioskAppManager::Get()->GetApp(app_id_, &app));

  int auto_launch_delay = -1;
  if (is_auto_launch) {
    if (!CrosSettings::Get()->GetInteger(
            kAccountsPrefDeviceLocalAccountAutoLoginDelay,
            &auto_launch_delay)) {
      auto_launch_delay = 0;
    }
    DCHECK_EQ(0, auto_launch_delay)
        << "Kiosks do not support non-zero auto-login delays";

    // If we are launching a kiosk app with zero delay, mark it appropriately.
    if (auto_launch_delay == 0)
      KioskAppManager::Get()->SetAppWasAutoLaunchedWithZeroDelay(app_id_);
  }

  extensions::SetCurrentFeatureSessionType(
      is_auto_launch && auto_launch_delay == 0
          ? extensions::FeatureSessionType::AUTOLAUNCHED_KIOSK
          : extensions::FeatureSessionType::KIOSK);

  kiosk_profile_loader_.reset(
      new KioskProfileLoader(app.account_id, false, this));
  kiosk_profile_loader_->Start();
}

// static
void AppLaunchController::SkipSplashWaitForTesting() {
  skip_splash_wait_ = true;
}

// static
void AppLaunchController::SetNetworkWaitForTesting(int wait_time_secs) {
  network_wait_time_ = wait_time_secs;
}

// static
void AppLaunchController::SetNetworkTimeoutCallbackForTesting(
    base::Closure* callback) {
  network_timeout_callback_ = callback;
}

// static
void AppLaunchController::SetCanConfigureNetworkCallbackForTesting(
    ReturnBoolCallback* can_configure_network_callback) {
  can_configure_network_callback_ = can_configure_network_callback;
}

// static
void AppLaunchController::SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
    ReturnBoolCallback* need_owner_auth_callback) {
  need_owner_auth_to_configure_network_callback_ = need_owner_auth_callback;
}

void AppLaunchController::OnConfigureNetwork() {
  DCHECK(profile_);
  if (showing_network_dialog_)
    return;

  showing_network_dialog_ = true;
  if (CanConfigureNetwork() && NeedOwnerAuthToConfigureNetwork()) {
    signin_screen_.reset(new AppLaunchSigninScreen(oobe_ui_, this));
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
  ShowNetworkConfigureUIWhenReady();
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

void AppLaunchController::OnNetworkConfigRequested(bool requested) {
  network_config_requested_ = requested;
  if (requested) {
    MaybeShowNetworkConfigureUI();
  } else {
    app_launch_splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::APP_LAUNCH_STATE_PREPARING_NETWORK);
    startup_app_launcher_->RestartLauncher();
  }
}

void AppLaunchController::OnNetworkStateChanged(bool online) {
  if (!waiting_for_network_)
    return;

  if (online && !network_config_requested_)
    startup_app_launcher_->ContinueWithNetworkReady();
  else if (network_wait_timedout_)
    MaybeShowNetworkConfigureUI();
}

void AppLaunchController::OnProfileLoaded(Profile* profile) {
  DVLOG(1) << "Profile loaded... Starting app launch.";
  profile_ = profile;

  // This is needed to trigger input method extensions being loaded.
  profile_->InitChromeOSPreferences();

  kiosk_profile_loader_.reset();
  startup_app_launcher_.reset(
      new StartupAppLauncher(profile_, app_id_, diagnostic_mode_, this));
  startup_app_launcher_->Initialize();

  if (show_network_config_ui_after_profile_load_)
    ShowNetworkConfigureUIWhenReady();
}

void AppLaunchController::OnProfileLoadFailed(
    KioskAppLaunchError::Error error) {
  OnLaunchFailed(error);
}

void AppLaunchController::ClearNetworkWaitTimer() {
  waiting_for_network_ = false;
  network_wait_timer_.Stop();
}

void AppLaunchController::CleanUp() {
  ClearNetworkWaitTimer();
  kiosk_profile_loader_.reset();
  startup_app_launcher_.reset();
  splash_wait_timer_.Stop();

  if (host_)
    host_->Finalize();
}

void AppLaunchController::OnNetworkWaitTimedout() {
  DCHECK(waiting_for_network_);
  LOG(WARNING) << "OnNetworkWaitTimedout... connection = "
               <<  net::NetworkChangeNotifier::GetConnectionType();
  network_wait_timedout_ = true;

  MaybeShowNetworkConfigureUI();

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

  if (IsEnterpriseManaged()) {
    bool should_prompt;
    if (CrosSettings::Get()->GetBoolean(
            kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
            &should_prompt)) {
      return should_prompt;
    }

    // Default to true to allow network configuration if the policy is missing.
    return true;
  }

  return user_manager::UserManager::Get()->GetOwnerAccountId().is_valid();
}

bool AppLaunchController::NeedOwnerAuthToConfigureNetwork() {
  if (need_owner_auth_to_configure_network_callback_)
    return need_owner_auth_to_configure_network_callback_->Run();

  return !IsEnterpriseManaged();
}

void AppLaunchController::MaybeShowNetworkConfigureUI() {
  if (CanConfigureNetwork()) {
    if (NeedOwnerAuthToConfigureNetwork()) {
      if (network_config_requested_)
        OnConfigureNetwork();
      else
        app_launch_splash_screen_view_->ToggleNetworkConfig(true);
    } else {
      ShowNetworkConfigureUIWhenReady();
    }
  } else {
    app_launch_splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::APP_LAUNCH_STATE_NETWORK_WAIT_TIMEOUT);
  }
}

void AppLaunchController::ShowNetworkConfigureUIWhenReady() {
  if (!profile_) {
    show_network_config_ui_after_profile_load_ = true;
    app_launch_splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::
            APP_LAUNCH_STATE_SHOWING_NETWORK_CONFIGURE_UI);
    return;
  }

  show_network_config_ui_after_profile_load_ = false;
  showing_network_dialog_ = true;
  app_launch_splash_screen_view_->ShowNetworkConfigureUI();
}

void AppLaunchController::InitializeNetwork() {
  // Show the network configuration dialog if network is not initialized
  // after a brief wait time.
  waiting_for_network_ = true;
  network_wait_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(network_wait_time_),
      this, &AppLaunchController::OnNetworkWaitTimedout);

  app_launch_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_PREPARING_NETWORK);
}

bool AppLaunchController::IsNetworkReady() {
  return app_launch_splash_screen_view_->IsNetworkReady();
}

bool AppLaunchController::ShouldSkipAppInstallation() {
  return false;
}

void AppLaunchController::OnLoadingOAuthFile() {
  app_launch_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_LOADING_AUTH_FILE);
}

void AppLaunchController::OnInitializingTokenService() {
  app_launch_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_LOADING_TOKEN_SERVICE);
}

void AppLaunchController::OnInstallingApp() {
  app_launch_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_INSTALLING_APPLICATION);

  ClearNetworkWaitTimer();
  app_launch_splash_screen_view_->ToggleNetworkConfig(false);

  // We have connectivity at this point, so we can skip the network
  // configuration dialog if it is being shown.
  if (showing_network_dialog_) {
    app_launch_splash_screen_view_->Show(app_id_);
    showing_network_dialog_ = false;
    launch_splash_start_time_ = base::TimeTicks::Now().ToInternalValue();
  }
}

void AppLaunchController::OnReadyToLaunch() {
  launcher_ready_ = true;

  if (network_config_requested_)
    return;

  if (!webui_visible_)
    return;

  if (splash_wait_timer_.IsRunning())
    return;

  ClearNetworkWaitTimer();

  const int64_t time_taken_ms =
      (base::TimeTicks::Now() -
       base::TimeTicks::FromInternalValue(launch_splash_start_time_))
          .InMilliseconds();

  // Enforce that we show app install splash screen for some minimum amount
  // of time.
  if (!skip_splash_wait_ && time_taken_ms < kAppInstallSplashScreenMinTimeMS) {
    splash_wait_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(
            kAppInstallSplashScreenMinTimeMS - time_taken_ms),
        this,
        &AppLaunchController::OnReadyToLaunch);
    return;
  }

  startup_app_launcher_->LaunchApp();
}

void AppLaunchController::OnLaunchSucceeded() {
  DVLOG(1) << "Kiosk launch succeeded, wait for app window.";
  app_launch_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::APP_LAUNCH_STATE_WAITING_APP_WINDOW);

  DCHECK(!app_window_watcher_);
  app_window_watcher_.reset(new AppWindowWatcher(this, app_id_));
}

void AppLaunchController::OnLaunchFailed(KioskAppLaunchError::Error error) {
  DCHECK_NE(KioskAppLaunchError::NONE, error);
  LOG(ERROR) << "Kiosk launch failed, error=" << error;

  // Reboot on the recoverable cryptohome errors.
  if (error == KioskAppLaunchError::CRYPTOHOMED_NOT_RUNNING ||
      error == KioskAppLaunchError::ALREADY_MOUNTED) {
    // Do not save the error because saved errors would stop app from launching
    // on the next run.
    chrome::AttemptRelaunch();
    return;
  }

  // Saves the error and ends the session to go back to login screen.
  KioskAppLaunchError::Save(error);
  chrome::AttemptUserExit();
  CleanUp();
}

bool AppLaunchController::IsShowingNetworkConfigScreen() {
  return network_config_requested_;
}

}   // namespace chromeos
