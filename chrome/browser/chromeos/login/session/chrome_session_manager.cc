// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/chrome_session_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/lock/webui_screen_locker.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/tether/tether_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"

namespace chromeos {

namespace {

// Whether kiosk auto launch should be started.
bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line) {
  KioskAppManager* app_manager = KioskAppManager::Get();
  return command_line.HasSwitch(switches::kLoginManager) &&
         !command_line.HasSwitch(switches::kForceLoginManagerInTests) &&
         app_manager->IsAutoLaunchEnabled() &&
         KioskAppLaunchError::Get() == KioskAppLaunchError::NONE;
}

// Starts kiosk app auto launch and shows the splash screen.
void StartKioskSession() {
  // Kiosk app launcher starts with login state.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  ShowLoginWizard(chromeos::OobeScreen::SCREEN_APP_LAUNCH_SPLASH);

  // Login screen is skipped but 'login-prompt-visible' signal is still needed.
  VLOG(1) << "Kiosk app auto launch >> login-prompt-visible";
  DBusThreadManager::Get()->GetSessionManagerClient()->EmitLoginPromptVisible();
}

// Starts the login/oobe screen.
void StartLoginOobeSession() {
  // State will be defined once out-of-box/login branching is complete.
  ShowLoginWizard(OobeScreen::SCREEN_UNKNOWN);

  // Reset reboot after update flag when login screen is shown.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->ClearPref(prefs::kRebootAfterUpdate);
  }
}

// Starts Chrome with an existing user session. Possible cases:
// 1. Chrome is restarted after crash.
// 2. Chrome is restarted for Guest session.
// 3. Chrome is started in browser_tests skipping the login flow.
// 4. Chrome is started on dev machine i.e. not on Chrome OS device w/o
//    login flow. In that case --login-user=[user_manager::kStubUserEmail] is
//    added. See PreEarlyInitialization().
void StartUserSession(Profile* user_profile, const std::string& login_user_id) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(chromeos::switches::kLoginUser)) {
    // This is done in SessionManager::OnProfileCreated during normal login.
    UserSessionManager* user_session_mgr = UserSessionManager::GetInstance();
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    const user_manager::User* user = user_manager->GetActiveUser();
    if (!user) {
      // This is possible if crash occured after profile removal
      // (see crbug.com/178290 for some more info).
      LOG(ERROR) << "Could not get active user after crash.";
      return;
    }
    user_session_mgr->InitRlz(user_profile);
    user_session_mgr->InitializeCerts(user_profile);
    user_session_mgr->InitializeCRLSetFetcher(user);
    user_session_mgr->InitializeCertificateTransparencyComponents(user);
    if (lock_screen_apps::StateController::IsEnabled())
      lock_screen_apps::StateController::Get()->SetPrimaryProfile(user_profile);

    arc::ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(user_profile);

    TetherService* tether_service = TetherService::Get(user_profile);
    if (tether_service)
      tether_service->StartTetherIfEnabled();

    // Send the PROFILE_PREPARED notification and call SessionStarted()
    // so that the Launcher and other Profile dependent classes are created.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(user_profile));

    // This call will set session state to SESSION_STATE_ACTIVE (same one).
    session_manager::SessionManager::Get()->SessionStarted();

    // Now is the good time to retrieve other logged in users for this session.
    // First user has been already marked as logged in and active in
    // PreProfileInit(). Restore sessions for other users in the background.
    user_session_mgr->RestoreActiveSessions();
  }

  bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                         command_line->HasSwitch(::switches::kTestType);
  if (!is_running_test) {
    // Enable CrasAudioHandler logging when chrome restarts after crashing.
    if (chromeos::CrasAudioHandler::IsInitialized())
      chromeos::CrasAudioHandler::Get()->LogErrors();

    // We did not log in (we crashed or are debugging), so we need to
    // restore Sync.
    UserSessionManager::GetInstance()->RestoreAuthenticationSession(
        user_profile);
  }
}

// Starts a user session with stub user. This also happens on a dev machine
// when running Chrome w/o login flow. See PreEarlyInitialization().
void StartStubLoginSession(Profile* user_profile,
                           const std::string& login_user_id) {
  // For dev machines and stub user emulate as if sync has been initialized.
  SigninManagerFactory::GetForProfile(user_profile)
      ->SetAuthenticatedAccountInfo(login_user_id, login_user_id);
  StartUserSession(user_profile, login_user_id);
}

}  // namespace

ChromeSessionManager::ChromeSessionManager() {}
ChromeSessionManager::~ChromeSessionManager() {}

void ChromeSessionManager::Initialize(
    const base::CommandLine& parsed_command_line,
    Profile* profile,
    bool is_running_test) {
  // Keep Chrome alive for mash.
  // TODO(xiyuan): Remove this when session manager is moved out of Chrome.
  if (ash_util::IsRunningInMash() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableZeroBrowsersOpenForTests)) {
    g_browser_process->platform_part()->RegisterKeepAlive();
  }

  // Tests should be able to tune login manager before showing it. Thus only
  // show login UI (login and out-of-box) in normal (non-testing) mode with
  // --login-manager switch and if test passed --force-login-manager-in-tests.
  bool force_login_screen_in_test =
      parsed_command_line.HasSwitch(switches::kForceLoginManagerInTests);

  const std::string cryptohome_id =
      parsed_command_line.GetSwitchValueASCII(switches::kLoginUser);
  const AccountId login_account_id(
      cryptohome::Identification::FromString(cryptohome_id).GetAccountId());

  KioskAppManager::RemoveObsoleteCryptohomes();
  ArcKioskAppManager::RemoveObsoleteCryptohomes();

  if (ShouldAutoLaunchKioskApp(parsed_command_line)) {
    VLOG(1) << "Starting Chrome with kiosk auto launch.";
    StartKioskSession();
    return;
  }

  if (parsed_command_line.HasSwitch(switches::kLoginManager) &&
      (!is_running_test || force_login_screen_in_test)) {
    VLOG(1) << "Starting Chrome with login/oobe screen.";
    StartLoginOobeSession();
    return;
  }

  if (!base::SysInfo::IsRunningOnChromeOS() &&
      login_account_id == user_manager::StubAccountId()) {
    VLOG(1) << "Starting Chrome with stub login.";
    StartStubLoginSession(profile, login_account_id.GetUserEmail());
    return;
  }

  VLOG(1) << "Starting Chrome with a user session.";
  StartUserSession(profile, login_account_id.GetUserEmail());
}

void ChromeSessionManager::SessionStarted() {
  session_manager::SessionManager::SessionStarted();
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Notifies UserManager so that it can update login state.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager)
    user_manager->OnSessionStarted();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::Source<session_manager::SessionManager>(this),
      content::Details<const user_manager::User>(
          user_manager->GetActiveUser()));

  chromeos::WebUIScreenLocker::RequestPreload();
}

void ChromeSessionManager::NotifyUserLoggedIn(const AccountId& user_account_id,
                                              const std::string& user_id_hash,
                                              bool browser_restart) {
  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("UserLoggedIn-Start", false);
  session_manager::SessionManager::NotifyUserLoggedIn(
      user_account_id, user_id_hash, browser_restart);
  btl->AddLoginTimeMarker("UserLoggedIn-End", false);
}

}  // namespace chromeos
