// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/mock_login_utils.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace testing;

namespace {

const char* kTestUsers[] = {"test-user@gmail.com",
                            "test-user1@gmail.com",
                            "test-user2@gmail.com"};

}  // anonymous namespace

namespace chromeos {

class UserAddingScreenTest : public InProcessBrowserTest,
                             public UserAddingScreen::Observer {
 public:
  UserAddingScreenTest() : user_adding_started_(0), user_adding_finished_(0) {
    set_exit_when_last_browser_closes(false);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(::switches::kMultiProfiles);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    UserAddingScreen::Get()->AddObserver(this);
    mock_login_utils_ = new NiceMock<MockLoginUtils>();
    mock_login_utils_->DelegateToFake();
    LoginUtils::Set(mock_login_utils_);
  }

  virtual void OnUserAddingFinished() OVERRIDE { ++user_adding_finished_; }

  virtual void OnUserAddingStarted() OVERRIDE { ++user_adding_started_; }

  void RegisterUser(const std::string& username) {
    ListPrefUpdate users_pref(g_browser_process->local_state(),
                              "LoggedInUsers");
    users_pref->AppendIfNotPresent(new base::StringValue(username));
  }

  void SetExpectedCredentials(const std::string& username,
                              const std::string& password) {
    mock_login_utils_->GetFakeLoginUtils()->
        SetExpectedCredentials(username, password);
  }

  bool TryToLogin(const std::string& username, const std::string& password) {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    EXPECT_TRUE(controller != NULL);
    controller->Login(UserContext(username, password, ""));
    content::RunAllPendingInMessageLoop();
    return UserManager::Get()->GetActiveUser()->email() == username;
  }

  void LoginUser(const std::string& username) {
    SetExpectedCredentials(username, "password");
    EXPECT_TRUE(TryToLogin(username, "password"));
  }

  int user_adding_started() { return user_adding_started_; }

  int user_adding_finished() { return user_adding_finished_; }

  MockLoginUtils& login_utils() { return *mock_login_utils_; }

 private:
  int user_adding_started_;
  int user_adding_finished_;
  MockLoginUtils* mock_login_utils_;

  DISALLOW_COPY_AND_ASSIGN(UserAddingScreenTest);
};

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, PRE_CancelAdding) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  RegisterUser(kTestUsers[2]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, CancelAdding) {
  EXPECT_CALL(login_utils(), DoBrowserLaunch(_, _)).Times(1);
  EXPECT_EQ(UserManager::Get()->GetUsers().size(), 3u);
  EXPECT_EQ(UserManager::Get()->GetLoggedInUsers().size(), 0u);

  LoginUser(kTestUsers[0]);
  EXPECT_EQ(UserManager::Get()->GetLoggedInUsers().size(), 1u);

  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(user_adding_started(), 1);

  UserAddingScreen::Get()->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(user_adding_finished(), 1);

  EXPECT_TRUE(LoginDisplayHostImpl::default_host() == NULL);
  EXPECT_EQ(UserManager::Get()->GetLoggedInUsers().size(), 1u);
  EXPECT_EQ(UserManager::Get()->GetActiveUser()->email(), kTestUsers[0]);
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, PRE_AddingSeveralUsers) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  RegisterUser(kTestUsers[2]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(UserAddingScreenTest, AddingSeveralUsers) {
  EXPECT_CALL(login_utils(), DoBrowserLaunch(_, _)).Times(3);
  LoginUser(kTestUsers[0]);
  for (int i = 1; i < 3; ++i) {
    UserAddingScreen::Get()->Start();
    content::RunAllPendingInMessageLoop();
    EXPECT_EQ(user_adding_started(), i);
    LoginUser(kTestUsers[i]);
    EXPECT_EQ(user_adding_finished(), i);
    EXPECT_TRUE(LoginDisplayHostImpl::default_host() == NULL);
    EXPECT_EQ(UserManager::Get()->GetLoggedInUsers().size(), unsigned(i + 1));
  }
}

}  // namespace chromeos
