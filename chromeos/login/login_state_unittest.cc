// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/login_state.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class LoginStateTest : public testing::Test,
                       public LoginState::Observer {
 public:
  LoginStateTest()
      : logged_in_state_(LoginState::LOGGED_IN_OOBE),
        logged_in_user_type_(LoginState::LOGGED_IN_USER_NONE) {
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
  virtual void LoggedInStateChanged(LoginState::LoggedInState state) OVERRIDE {
    logged_in_state_ = state;
    logged_in_user_type_ = LoginState::Get()->GetLoggedInUserType();
  }

 protected:
  MessageLoopForUI message_loop_;
  LoginState::LoggedInState logged_in_state_;
  LoginState::LoggedInUserType logged_in_user_type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginStateTest);
};

TEST_F(LoginStateTest, TestLoginState) {
  EXPECT_EQ(LoginState::LOGGED_IN_OOBE, logged_in_state_);
  EXPECT_EQ(LoginState::LOGGED_IN_OOBE, LoginState::Get()->GetLoggedInState());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE, logged_in_user_type_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_NONE,
            LoginState::Get()->GetLoggedInUserType());
  // Setting login state to ACTIVE.
  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_REGULAR);
  EXPECT_EQ(LoginState::LOGGED_IN_ACTIVE,
            LoginState::Get()->GetLoggedInState());
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR,
            LoginState::Get()->GetLoggedInUserType());
  EXPECT_TRUE(LoginState::Get()->IsUserLoggedIn());
  // Run the message loop, observer should update members.
  message_loop_.RunUntilIdle();
  EXPECT_EQ(LoginState::LOGGED_IN_ACTIVE, logged_in_state_);
  EXPECT_EQ(LoginState::LOGGED_IN_USER_REGULAR, logged_in_user_type_);
}

}  // namespace
