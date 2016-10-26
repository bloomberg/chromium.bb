// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/chrome_session_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/session/kiosk_auto_launcher_session_manager_delegate.h"
#include "chrome/browser/chromeos/login/session/login_oobe_session_manager_delegate.h"
#include "chrome/browser/chromeos/login/session/restore_after_crash_session_manager_delegate.h"
#include "chrome/browser/chromeos/login/session/stub_login_session_manager_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/login/user_names.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line) {
  KioskAppManager* app_manager = KioskAppManager::Get();
  return command_line.HasSwitch(switches::kLoginManager) &&
         !command_line.HasSwitch(switches::kForceLoginManagerInTests) &&
         app_manager->IsAutoLaunchEnabled() &&
         KioskAppLaunchError::Get() == KioskAppLaunchError::NONE;
}

}  // namespace

// static
std::unique_ptr<session_manager::SessionManager>
ChromeSessionManager::CreateSessionManager(
    const base::CommandLine& parsed_command_line,
    Profile* profile,
    bool is_running_test) {
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

  if (ShouldAutoLaunchKioskApp(parsed_command_line)) {
    VLOG(1) << "Starting Chrome with KioskAutoLauncherSessionManagerDelegate";
    return std::unique_ptr<session_manager::SessionManager>(
        new ChromeSessionManager(
            new KioskAutoLauncherSessionManagerDelegate()));
  } else if (parsed_command_line.HasSwitch(switches::kLoginManager) &&
             (!is_running_test || force_login_screen_in_test)) {
    VLOG(1) << "Starting Chrome with LoginOobeSessionManagerDelegate";
    return std::unique_ptr<session_manager::SessionManager>(
        new ChromeSessionManager(new LoginOobeSessionManagerDelegate()));
  } else if (!base::SysInfo::IsRunningOnChromeOS() &&
             login_account_id == login::StubAccountId()) {
    VLOG(1) << "Starting Chrome with StubLoginSessionManagerDelegate";
    return std::unique_ptr<session_manager::SessionManager>(
        new ChromeSessionManager(new StubLoginSessionManagerDelegate(
            profile, login_account_id.GetUserEmail())));
  } else {
    VLOG(1) << "Starting Chrome with  RestoreAfterCrashSessionManagerDelegate";
    // Restarting Chrome inside existing user session. Possible cases:
    // 1. Chrome is restarted after crash.
    // 2. Chrome is restarted for Guest session.
    // 3. Chrome is started in browser_tests skipping the login flow.
    // 4. Chrome is started on dev machine i.e. not on Chrome OS device w/o
    //    login flow. In that case --login-user=[chromeos::login::kStubUser] is
    //    added. See PreEarlyInitialization().
    return std::unique_ptr<session_manager::SessionManager>(
        new ChromeSessionManager(new RestoreAfterCrashSessionManagerDelegate(
            profile, login_account_id.GetUserEmail())));
  }
}

ChromeSessionManager::ChromeSessionManager(
    session_manager::SessionManagerDelegate* delegate) {
  Initialize(delegate);
}

ChromeSessionManager::~ChromeSessionManager() {
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
}

}  // namespace chromeos
