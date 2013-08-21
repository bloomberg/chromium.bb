// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_manager_test.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

LoginManagerTest::LoginManagerTest(bool should_launch_browser)
    : should_launch_browser_(should_launch_browser) {
  set_exit_when_last_browser_closes(false);
}

void LoginManagerTest::SetUpCommandLine(CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  command_line->AppendSwitch(::switches::kMultiProfiles);
}

void LoginManagerTest::SetUpInProcessBrowserTestFixture() {
  mock_login_utils_ = new testing::NiceMock<MockLoginUtils>();
  mock_login_utils_->DelegateToFake();
  mock_login_utils_->GetFakeLoginUtils()->set_should_launch_browser(
      should_launch_browser_);
  LoginUtils::Set(mock_login_utils_);
}

void LoginManagerTest::RegisterUser(const std::string& username) {
  ListPrefUpdate users_pref(g_browser_process->local_state(), "LoggedInUsers");
  users_pref->AppendIfNotPresent(new base::StringValue(username));
}

void LoginManagerTest::SetExpectedCredentials(const std::string& username,
                                              const std::string& password) {
  login_utils().GetFakeLoginUtils()->SetExpectedCredentials(username, password);
}

bool LoginManagerTest::TryToLogin(const std::string& username,
                                  const std::string& password) {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  EXPECT_TRUE(controller != NULL);
  controller->Login(UserContext(username, password, std::string()));
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
  if (const User* active_user = UserManager::Get()->GetActiveUser())
    return active_user->email() == username;
  return false;
}

void LoginManagerTest::LoginUser(const std::string& username) {
  SetExpectedCredentials(username, "password");
  EXPECT_TRUE(TryToLogin(username, "password"));
}

}  // namespace chromeos
