// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/login_state.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class LoginStateTest : public testing::Test,
                       public LoginState::Observer {
 public:
  LoginStateTest() : logged_in_user_type_(LoginState::LOGGED_IN_USER_NONE),
                     login_state_changes_count_(0) {
  }
  virtual ~LoginStateTest() {
  }

  // testing::Test
  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kLoginManager);
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
    LoginState::Initialize();
    LoginState::Get()->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    LoginState::Get()->RemoveObserver(this);
    LoginState::Shutdown();
    DBusThreadManager::Shutdown();
  }

  // LoginState::Observer
  virtual void LoggedInStateChanged() OVERRIDE {
    ++login_state_changes_count_;
    logged_in_user_type_ = LoginState::Get()->GetLoggedInUserType();
  }

 protected:
  // Returns number of times the login state changed since the last call to
  // this method.
  unsigned int GetNewLoginStateChangesCount() {
    unsigned int result = login_state_changes_count_;
    login_state_changes_count_ = 0;
    return result;
  }

  base::MessageLoopForUI message_loop_;
  LoginState::LoggedInUserType logged_in_user_type_;

 private:
  unsigned int login_state_changes_count_;

  DISALLOW_COPY_AND_ASSIGN(LoginStateTest);
};

TEST_F(LoginStateTest, TestLoginState) {
  EXPECT_FALSE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE, logged_in_user_type_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE,
            LoginState::Get()->GetLoggedInUserType());

  // Setting login state to ACTIVE.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_REGULAR);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR,
            LoginState::Get()->GetLoggedInUserType());
  EXPECT_TRUE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());

  // Run the message loop, observer should update members.
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1U, GetNewLoginStateChangesCount());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR, logged_in_user_type_);
}

TEST_F(LoginStateTest, TestSafeModeLoginState) {
  EXPECT_FALSE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE, logged_in_user_type_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE,
            LoginState::Get()->GetLoggedInUserType());
  // Setting login state to SAFE MODE.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_SAFE_MODE,
                                      LoginState::LOGGED_IN_USER_NONE);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE,
            LoginState::Get()->GetLoggedInUserType());
  EXPECT_FALSE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_TRUE(LoginState::Get()->IsInSafeMode());

  // Run the message loop, observer should update members.
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1U, GetNewLoginStateChangesCount());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE, logged_in_user_type_);

  // Setting login state to ACTIVE.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_OWNER);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_OWNER,
            LoginState::Get()->GetLoggedInUserType());
  EXPECT_TRUE(LoginState::Get()->IsUserLoggedIn());
  EXPECT_FALSE(LoginState::Get()->IsInSafeMode());

  // Run the message loop, observer should update members.
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1U, GetNewLoginStateChangesCount());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_OWNER, logged_in_user_type_);
}

}  // namespace chromeos
