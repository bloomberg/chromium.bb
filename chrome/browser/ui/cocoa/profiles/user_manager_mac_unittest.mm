// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"

class UserManagerMacTest : public BrowserWithTestWindowTest {
 public:
  UserManagerMacTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    // Create a user to make sure the System Profile isn't the only one since it
    // shouldn't be added to the ProfileInfoCache.
    testing_profile_manager_.CreateTestingProfile("Default");
    // Pre-load the system profile so we don't have to wait for the User Manager
    // to asynchronously create it.
    testing_profile_manager_.CreateSystemProfile();
  }

  void TearDown() override {
    testing_profile_manager_.DeleteSystemProfile();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
    base::RunLoop().RunUntilIdle();
    BrowserWithTestWindowTest::TearDown();
  }

 private:
  TestingProfileManager testing_profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerMacTest);
};

TEST_F(UserManagerMacTest, ShowUserManager) {
  // Set the ProfileLastUsed pref so that SetActiveProfileToGuestIfLocked()
  // uses a last active profile that's in the ProfileInfoCache, not default.
  g_browser_process->local_state()->SetString(
      prefs::kProfileLastUsed,
      g_browser_process->profile_manager()->GetProfileInfoCache().
          GetPathOfProfileAtIndex(0).BaseName().MaybeAsASCII());

  EXPECT_FALSE(UserManager::IsShowing());
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_NO_TUTORIAL,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
  EXPECT_TRUE(UserManager::IsShowing());

  UserManager::Hide();
  EXPECT_FALSE(UserManager::IsShowing());
}
