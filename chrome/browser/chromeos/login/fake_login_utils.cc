// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/fake_login_utils.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

FakeLoginUtils::FakeLoginUtils() : should_launch_browser_(false) {}

FakeLoginUtils::~FakeLoginUtils() {}

void FakeLoginUtils::DoBrowserLaunch(Profile* profile,
                                     LoginDisplayHost* login_host) {

  if (!UserManager::Get()->GetCurrentUserFlow()->ShouldLaunchBrowser()) {
      UserManager::Get()->GetCurrentUserFlow()->LaunchExtraSteps(profile);
      return;
  }
  login_host->BeforeSessionStart();
  if (should_launch_browser_) {
    StartupBrowserCreator browser_creator;
    chrome::startup::IsFirstRun first_run =
        first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                      : chrome::startup::IS_NOT_FIRST_RUN;
    ASSERT_TRUE(
        browser_creator.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                                      profile,
                                      base::FilePath(),
                                      chrome::startup::IS_PROCESS_STARTUP,
                                      first_run,
                                      NULL));
  }
  if (login_host)
    login_host->Finalize();
  UserManager::Get()->SessionStarted();
}

void FakeLoginUtils::PrepareProfile(const UserContext& user_context,
                                    const std::string& display_email,
                                    bool has_cookies,
                                    bool has_active_session,
                                    LoginUtils::Delegate* delegate) {
  UserManager::Get()->UserLoggedIn(
      user_context.username, user_context.username_hash, false);
  User* user = UserManager::Get()->FindUserAndModify(user_context.username);
  DCHECK(user);

  // Make sure that we get the real Profile instead of the login Profile.
  user->set_profile_is_created();
  Profile* profile = UserManager::Get()->GetProfileByUser(user);

  if (UserManager::Get()->IsLoggedInAsLocallyManagedUser()) {
    User* active_user = UserManager::Get()->GetActiveUser();
    std::string managed_user_sync_id =
        UserManager::Get()->GetSupervisedUserManager()->
            GetUserSyncId(active_user->email());
    if (managed_user_sync_id.empty())
      managed_user_sync_id = "DUMMY ID";
    profile->GetPrefs()->SetString(prefs::kManagedUserId,
                                   managed_user_sync_id);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));
  if (delegate)
    delegate->OnProfilePrepared(profile);
}

void FakeLoginUtils::DelegateDeleted(LoginUtils::Delegate* delegate) {
  NOTREACHED() << "Method not implemented.";
}

void FakeLoginUtils::CompleteOffTheRecordLogin(const GURL& start_url) {
  NOTREACHED() << "Method not implemented.";
}

void FakeLoginUtils::SetFirstLoginPrefs(PrefService* prefs) {
  NOTREACHED() << "Method not implemented.";
}

scoped_refptr<Authenticator> FakeLoginUtils::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  authenticator_ =
      new MockAuthenticator(consumer, expected_username_, expected_password_);
  return authenticator_;
}

void FakeLoginUtils::RestoreAuthenticationSession(Profile* profile) {
  NOTREACHED() << "Method not implemented.";
}

void FakeLoginUtils::InitRlzDelayed(Profile* user_profile) {
  NOTREACHED() << "Method not implemented.";
}

void FakeLoginUtils::SetExpectedCredentials(const std::string& username,
                                            const std::string& password) {
  expected_username_ = username;
  expected_password_ = password;
  if (authenticator_.get()) {
    static_cast<MockAuthenticator*>(authenticator_.get())->
        SetExpectedCredentials(username, password);
  }
}

}  //  namespace chromeos
