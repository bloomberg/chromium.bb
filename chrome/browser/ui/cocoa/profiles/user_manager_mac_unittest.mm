// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"

class UserManagerMacTest : public BrowserWithTestWindowTest {
 public:
  UserManagerMacTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    // Pre-load the guest profile so we don't have to wait for the User Manager
    // to asynchronously create it.
    testing_profile_manager_.CreateGuestProfile();
  }

  virtual void TearDown() OVERRIDE {
    testing_profile_manager_.DeleteGuestProfile();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
    base::RunLoop().RunUntilIdle();
    BrowserWithTestWindowTest::TearDown();
  }

 private:
  TestingProfileManager testing_profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerMacTest);
};

TEST_F(UserManagerMacTest, ShowUserManager) {
  EXPECT_FALSE(UserManagerMac::IsShowing());
  UserManagerMac::Show(base::FilePath(), profiles::USER_MANAGER_NO_TUTORIAL);
  EXPECT_TRUE(UserManagerMac::IsShowing());

  UserManagerMac::Hide();
  EXPECT_FALSE(UserManagerMac::IsShowing());
}
