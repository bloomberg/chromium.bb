// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller_delegate.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kOwner = "owner@gmail.com";
const char* kUsers[] = {"a@gmail.com", "b@gmail.com", kOwner};

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

    for (size_t i = 0; i < arraysize(kUsers); ++i) {
      const std::string user_email(kUsers[i]);
      fake_user_manager_->AddUser(user_email);
    }
    fake_user_manager_->set_owner_email(kUsers[2]);
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

TEST_F(SigninPrepareUserListTest, BasicList) {
  UserList users_to_send = UserSelectionScreen::PrepareUserListForSending(
      fake_user_manager_->GetUsers(),
      kOwner,
      true /* is signin to add */);

  size_t list_length = 3;
  EXPECT_EQ(list_length, users_to_send.size());
}

}  // namespace chromeos
