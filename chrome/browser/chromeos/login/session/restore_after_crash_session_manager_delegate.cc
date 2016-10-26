// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/restore_after_crash_session_manager_delegate.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"

namespace chromeos {

RestoreAfterCrashSessionManagerDelegate::
    RestoreAfterCrashSessionManagerDelegate(Profile* profile,
                                            const std::string& login_user_id)
    : profile_(profile), login_user_id_(login_user_id) {}

RestoreAfterCrashSessionManagerDelegate::
    ~RestoreAfterCrashSessionManagerDelegate() {}

void RestoreAfterCrashSessionManagerDelegate::Start() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);

  // TODO(nkostylev): Identify tests that do not set this kLoginUser flag but
  // still rely on "stub user" session. Keeping existing behavior to avoid
  // breaking tests.
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
    user_session_mgr->InitRlz(profile());
    user_session_mgr->InitializeCerts(profile());
    user_session_mgr->InitializeCRLSetFetcher(user);
    user_session_mgr->InitializeCertificateTransparencyComponents(user);

    if (arc::ArcBridgeService::GetEnabled(
            base::CommandLine::ForCurrentProcess())) {
      DCHECK(arc::ArcServiceManager::Get());
      std::unique_ptr<BooleanPrefMember> arc_enabled_pref =
          base::MakeUnique<BooleanPrefMember>();
      arc_enabled_pref->Init(prefs::kArcEnabled, profile()->GetPrefs());
      arc::ArcServiceManager::Get()->OnPrimaryUserProfilePrepared(
          multi_user_util::GetAccountIdFromProfile(profile()),
          std::move(arc_enabled_pref));
      DCHECK(arc::ArcAuthService::Get());
      arc::ArcAuthService::Get()->OnPrimaryUserProfilePrepared(profile());
    }

    // Send the PROFILE_PREPARED notification and call SessionStarted()
    // so that the Launcher and other Profile dependent classes are created.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(profile()));

    // This call will set session state to SESSION_STATE_ACTIVE (same one).
    session_manager_->SessionStarted();

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
    UserSessionManager::GetInstance()->RestoreAuthenticationSession(profile());
  }
}

}  // namespace chromeos
