// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_session_state_delegate.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"

namespace test {

namespace {

const char kTestAccount1[] = "user1@test.com";
const char kTestAccount2[] = "user2@test.com";

}  // namespace

class BrowserFinderChromeOSTest : public BrowserWithTestWindowTest {
 protected:
  BrowserFinderChromeOSTest()
      : multi_user_window_manager_(nullptr),
        fake_user_manager_(new user_manager::FakeUserManager),
        user_manager_enabler_(fake_user_manager_) {}

  TestingProfile* CreateMultiUserProfile(const AccountId& account_id) {
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    GetUserWindowManager()->AddUser(profile);
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(
        const_cast<user_manager::User*>(user));
    ash::test::AshTestHelper::GetTestSessionStateDelegate()->AddUser(
        account_id);
    return profile;
  }

  chrome::MultiUserWindowManagerChromeOS* GetUserWindowManager() {
    if (!multi_user_window_manager_) {
      multi_user_window_manager_ =
          new chrome::MultiUserWindowManagerChromeOS(test_account_id1_);
      multi_user_window_manager_->Init();
      chrome::MultiUserWindowManager::SetInstanceForTest(
          multi_user_window_manager_,
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED);
    }
    return multi_user_window_manager_;
  }

  AccountId test_account_id1_ = EmptyAccountId();
  AccountId test_account_id2_ = EmptyAccountId();

 private:
  void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    test_account_id1_ = AccountId::FromUserEmail(kTestAccount1);
    test_account_id2_ = AccountId::FromUserEmail(kTestAccount2);
    profile_manager_->SetLoggedIn(true);
    chromeos::WallpaperManager::Initialize();
    BrowserWithTestWindowTest::SetUp();
    second_profile_ = CreateMultiUserProfile(test_account_id2_);
  }

  void TearDown() override {
    chrome::MultiUserWindowManager::DeleteInstance();
    BrowserWithTestWindowTest::TearDown();
    chromeos::WallpaperManager::Shutdown();
    if (second_profile_) {
      DestroyProfile(second_profile_);
      second_profile_ = nullptr;
    }
  }

  TestingProfile* CreateProfile() override {
    return CreateMultiUserProfile(test_account_id1_);
  }

  void DestroyProfile(TestingProfile* test_profile) override {
    profile_manager_->DeleteTestingProfile(test_profile->GetProfileUserName());
  }

  TestingProfile* second_profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager_;
  user_manager::FakeUserManager* fake_user_manager_;  // Not owned.
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFinderChromeOSTest);
};

TEST_F(BrowserFinderChromeOSTest, IncognitoBrowserMatchTest) {
  // GetBrowserCount() use kMatchAll to find all browser windows for profile().
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));
  set_browser(nullptr);

  // Create an incognito browser.
  Browser::CreateParams params(profile()->GetOffTheRecordProfile());
  std::unique_ptr<Browser> incognito_browser(
      chrome::CreateBrowserWithAuraTestWindowForParams(nullptr, &params));
  // Incognito windows are excluded in GetBrowserCount() because kMatchAll
  // doesn't match original profile of the browser with the given profile.
  EXPECT_EQ(0u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), false));
}

TEST_F(BrowserFinderChromeOSTest, FindBrowserOwnedByAnotherProfile) {
  set_browser(nullptr);

  Browser::CreateParams params(profile()->GetOriginalProfile());
  std::unique_ptr<Browser> browser(
      chrome::CreateBrowserWithAuraTestWindowForParams(nullptr, &params));
  GetUserWindowManager()->SetWindowOwner(browser->window()->GetNativeWindow(),
                                         test_account_id1_);
  EXPECT_EQ(1u, chrome::GetBrowserCount(profile()));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_TRUE(chrome::FindAnyBrowser(profile(), false));

  // Move the browser window to another user's desktop. Then no window should
  // be available for the current profile.
  GetUserWindowManager()->ShowWindowForUser(
      browser->window()->GetNativeWindow(), test_account_id2_);
  EXPECT_EQ(0u, chrome::GetBrowserCount(profile()));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), true));
  EXPECT_FALSE(chrome::FindAnyBrowser(profile(), false));
}

}  // namespace test
