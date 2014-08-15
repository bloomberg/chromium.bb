// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller_delegate.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kMaxUsers = 18; // same as in user_selection_screen.cc
const char* kOwner = "owner@gmail.com";
const char* kUsersPublic[] = {"public0@gmail.com", "public1@gmail.com"};
const char* kUsers[] = {
    "a0@gmail.com", "a1@gmail.com", "a2@gmail.com", "a3@gmail.com",
    "a4@gmail.com", "a5@gmail.com", "a6@gmail.com", "a7@gmail.com",
    "a8@gmail.com", "a9@gmail.com", "a10@gmail.com", "a11@gmail.com",
    "a12@gmail.com", "a13@gmail.com", "a14@gmail.com", "a15@gmail.com",
    "a16@gmail.com", "a17@gmail.com", kOwner, "a18@gmail.com"};

}  // namespace

namespace chromeos {

class SigninPrepareUserListTest
    : public testing::Test,
      public MultiProfileUserControllerDelegate {
 public:
  SigninPrepareUserListTest()
      : fake_user_manager_(new FakeUserManager()),
        user_manager_enabler_(fake_user_manager_) {
  }

  virtual ~SigninPrepareUserListTest() {
  }

  virtual void SetUp() OVERRIDE {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    controller_.reset(new MultiProfileUserController(
        this, TestingBrowserProcess::GetGlobal()->local_state()));
    fake_user_manager_->set_multi_profile_user_controller(controller_.get());

    for (size_t i = 0; i < arraysize(kUsersPublic); ++i)
      fake_user_manager_->AddPublicAccountUser(kUsersPublic[i]);

    for (size_t i = 0; i < arraysize(kUsers); ++i)
      fake_user_manager_->AddUser(kUsers[i]);

    fake_user_manager_->set_owner_email(kOwner);
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    profile_manager_.reset();
  }

  // MultiProfileUserControllerDelegate overrides:
  virtual void OnUserNotAllowed(const std::string& user_email) OVERRIDE {
  }

  FakeUserManager* fake_user_manager_;
  ScopedUserManagerEnabler user_manager_enabler_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  std::map<std::string,
           ScreenlockBridge::LockHandler::AuthType> user_auth_type_map;
  scoped_ptr<MultiProfileUserController> controller_;

  DISALLOW_COPY_AND_ASSIGN(SigninPrepareUserListTest);
};

TEST_F(SigninPrepareUserListTest, AlwaysKeepOwnerInList) {
  EXPECT_LT(kMaxUsers, fake_user_manager_->GetUsers().size());
  user_manager::UserList users_to_send =
      UserSelectionScreen::PrepareUserListForSending(
          fake_user_manager_->GetUsers(), kOwner, true /* is signin to add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ(kOwner, users_to_send.back()->email());

  fake_user_manager_->RemoveUserFromList("a16@gmail.com");
  fake_user_manager_->RemoveUserFromList("a17@gmail.com");
  users_to_send = UserSelectionScreen::PrepareUserListForSending(
      fake_user_manager_->GetUsers(),
      kOwner,
      true /* is signin to add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ("a18@gmail.com", users_to_send.back()->email());
  EXPECT_EQ(kOwner, users_to_send[kMaxUsers-2]->email());
}

TEST_F(SigninPrepareUserListTest, PublicAccounts) {
  user_manager::UserList users_to_send =
      UserSelectionScreen::PrepareUserListForSending(
          fake_user_manager_->GetUsers(), kOwner, true /* is signin to add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ("a0@gmail.com", users_to_send.front()->email());

  users_to_send = UserSelectionScreen::PrepareUserListForSending(
      fake_user_manager_->GetUsers(),
      kOwner,
      false /* is signin to add */);

  EXPECT_EQ(kMaxUsers, users_to_send.size());
  EXPECT_EQ("public0@gmail.com", users_to_send.front()->email());
}

}  // namespace chromeos
