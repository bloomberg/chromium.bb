// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

#include <string>
#include <vector>

#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"


class SessionStateDelegateChromeOSTest : public testing::Test {
 protected:
  SessionStateDelegateChromeOSTest() : user_manager_(NULL) {
  }

  virtual ~SessionStateDelegateChromeOSTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_ = new chromeos::FakeUserManager;
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(user_manager_));

    // Create our SessionStateDelegate to experiment with.
    session_state_delegate_.reset(new SessionStateDelegateChromeos());
    testing::Test::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    testing::Test::TearDown();
    session_state_delegate_.reset();
    user_manager_enabler_.reset();
    user_manager_ = NULL;
  }

  // Add and log in a user to the session.
  void UserAddedToSession(std::string user) {
    user_manager()->AddUser(user);
    user_manager()->LoginUser(user);
  }

  // Get the active user.
  const std::string& GetActiveUser() {
    return chromeos::UserManager::Get()->GetActiveUser()->email();
  }

  chromeos::FakeUserManager* user_manager() { return user_manager_; }
  SessionStateDelegateChromeos* session_state_delegate() {
    return session_state_delegate_.get();
  }

 private:
  scoped_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  scoped_ptr<SessionStateDelegateChromeos> session_state_delegate_;

  // Not owned.
  chromeos::FakeUserManager* user_manager_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateChromeOSTest);
};

// Make sure that cycling one user does not cause any harm.
TEST_F(SessionStateDelegateChromeOSTest, CyclingOneUser) {
  UserAddedToSession("firstuser@test.com");

  EXPECT_EQ("firstuser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(
      ash::SessionStateDelegate::CYCLE_TO_NEXT_USER);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(
      ash::SessionStateDelegate::CYCLE_TO_PREVIOUS_USER);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
}

// Cycle three users forwards and backwards to see that it works.
TEST_F(SessionStateDelegateChromeOSTest, CyclingThreeUsers) {
  UserAddedToSession("firstuser@test.com");
  UserAddedToSession("seconduser@test.com");
  UserAddedToSession("thirduser@test.com");
  const ash::SessionStateDelegate::CycleUser forward =
      ash::SessionStateDelegate::CYCLE_TO_NEXT_USER;

  // Cycle forward.
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("seconduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("thirduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(forward);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());

  // Cycle backwards.
  const ash::SessionStateDelegate::CycleUser backward =
      ash::SessionStateDelegate::CYCLE_TO_PREVIOUS_USER;
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("thirduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("seconduser@test.com", GetActiveUser());
  session_state_delegate()->CycleActiveUser(backward);
  EXPECT_EQ("firstuser@test.com", GetActiveUser());
}
