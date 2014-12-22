// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/fake_login_utils.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/login/auth/mock_authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

FakeLoginUtils::FakeLoginUtils() : should_launch_browser_(false) {}

FakeLoginUtils::~FakeLoginUtils() {}

void FakeLoginUtils::DoBrowserLaunch(Profile* profile,
                                     LoginDisplayHost* login_host) {
  if (!ChromeUserManager::Get()->GetCurrentUserFlow()->ShouldLaunchBrowser()) {
    ChromeUserManager::Get()->GetCurrentUserFlow()->LaunchExtraSteps(profile);
      return;
  }
  login_host->BeforeSessionStart();
  if (should_launch_browser_) {
    StartupBrowserCreator browser_creator;
    chrome::startup::IsFirstRun first_run =
        first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                      : chrome::startup::IS_NOT_FIRST_RUN;
    ASSERT_TRUE(browser_creator.LaunchBrowser(
        *base::CommandLine::ForCurrentProcess(), profile, base::FilePath(),
        chrome::startup::IS_PROCESS_STARTUP, first_run, NULL));
  }
  if (login_host)
    login_host->Finalize();
  user_manager::UserManager::Get()->SessionStarted();
}

void FakeLoginUtils::PrepareProfile(const UserContext& user_context,
                                    bool has_cookies,
                                    bool has_active_session,
                                    LoginUtils::Delegate* delegate) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  user_manager->UserLoggedIn(
      user_context.GetUserID(), user_context.GetUserIDHash(), false);
  user_manager::User* user =
      user_manager->FindUserAndModify(user_context.GetUserID());
  DCHECK(user);

  // Make sure that we get the real Profile instead of the login Profile.
  user->set_profile_is_created();
  Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  SigninManagerFactory::GetForProfile(profile)->SetAuthenticatedUsername(
      user_context.GetUserID());

  if (user_manager->IsLoggedInAsSupervisedUser()) {
    user_manager::User* active_user = user_manager->GetActiveUser();
    std::string supervised_user_sync_id =
        ChromeUserManager::Get()->GetSupervisedUserManager()->GetUserSyncId(
            active_user->email());
    if (supervised_user_sync_id.empty())
      supervised_user_sync_id = "DUMMY ID";
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user_sync_id);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));

  // Emulate UserSessionManager::InitializeUserSession() for now till
  // FakeLoginUtils are deprecated.
  bool browser_launched = false;
  if (!user_manager->IsLoggedInAsKioskApp()) {
    if (user_manager->IsCurrentUserNew()) {
      NOTREACHED() << "Method not implemented.";
    } else {
      browser_launched = true;
      LoginUtils::Get()->DoBrowserLaunch(profile,
                                         LoginDisplayHostImpl::default_host());
    }
  }

  if (delegate)
    delegate->OnProfilePrepared(profile, browser_launched);
}

void FakeLoginUtils::DelegateDeleted(LoginUtils::Delegate* delegate) {
  NOTREACHED() << "Method not implemented.";
}

scoped_refptr<Authenticator> FakeLoginUtils::CreateAuthenticator(
    AuthStatusConsumer* consumer) {
  authenticator_ = new MockAuthenticator(consumer, expected_user_context_);
  return authenticator_;
}

void FakeLoginUtils::SetExpectedCredentials(const UserContext& user_context) {
  expected_user_context_ = user_context;
  if (authenticator_.get()) {
    static_cast<MockAuthenticator*>(authenticator_.get())->
        SetExpectedCredentials(user_context);
  }
}

}  //  namespace chromeos
