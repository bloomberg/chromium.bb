// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

class AppControllerTest : public PlatformTest {
 protected:
  AppControllerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("New Profile 1");
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
    base::RunLoop().RunUntilIdle();
    PlatformTest::TearDown();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
};

TEST_F(AppControllerTest, DockMenu) {
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSMenu* menu = [ac applicationDockMenu:NSApp];
  NSMenuItem* item;

  EXPECT_TRUE(menu);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_WINDOW]);
  EXPECT_NE(-1, [menu indexOfItemWithTag:IDC_NEW_INCOGNITO_WINDOW]);
  for (item in [menu itemArray]) {
    EXPECT_EQ(ac.get(), [item target]);
    EXPECT_EQ(@selector(commandFromDock:), [item action]);
  }
}

TEST_F(AppControllerTest, LastProfile) {
  // Create a second profile.
  base::FilePath dest_path1 = profile_->GetPath();
  base::FilePath dest_path2 =
      profile_manager_.CreateTestingProfile("New Profile 2")->GetPath();
  ASSERT_EQ(2U, profile_manager_.profile_manager()->GetNumberOfProfiles());
  ASSERT_EQ(2U, profile_manager_.profile_manager()->GetLoadedProfiles().size());

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path1.BaseName().MaybeAsASCII());

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);

  // Delete the active profile.
  profile_manager_.profile_manager()->ScheduleProfileForDeletion(
      dest_path1, ProfileManager::CreateCallback());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dest_path2, [ac lastProfile]->GetPath());
}

TEST_F(AppControllerTest, TestSigninMenuItemNoErrors) {
  base::scoped_nsobject<NSMenuItem> syncMenuItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:@selector(commandDispatch)
                          keyEquivalent:@""]);
  [syncMenuItem setTag:IDC_SHOW_SYNC_SETUP];

  NSString* startSignin = l10n_util::GetNSStringFWithFixup(
      IDS_SYNC_MENU_PRE_SYNCED_LABEL,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  // Make sure shouldShow parameter is obeyed, and we get the default
  // label if not signed in.
  [AppController updateSigninItem:syncMenuItem
                       shouldShow:YES
                   currentProfile:profile_];

  EXPECT_TRUE([[syncMenuItem title] isEqualTo:startSignin]);
  EXPECT_FALSE([syncMenuItem isHidden]);

  [AppController updateSigninItem:syncMenuItem
                       shouldShow:NO
                   currentProfile:profile_];
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:startSignin]);
  EXPECT_TRUE([syncMenuItem isHidden]);

  // Now sign in.
  std::string username = "foo@example.com";
  NSString* alreadySignedIn = l10n_util::GetNSStringFWithFixup(
      IDS_SYNC_MENU_SYNCED_LABEL, base::UTF8ToUTF16(username));
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  signin->SetAuthenticatedAccountInfo(username, username);
  ProfileSyncService* sync = ProfileSyncServiceFactory::GetForProfile(profile_);
  sync->SetSyncSetupCompleted();
  [AppController updateSigninItem:syncMenuItem
                       shouldShow:YES
                   currentProfile:profile_];
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:alreadySignedIn]);
  EXPECT_FALSE([syncMenuItem isHidden]);
}

TEST_F(AppControllerTest, TestSigninMenuItemAuthError) {
  base::scoped_nsobject<NSMenuItem> syncMenuItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:@selector(commandDispatch)
                          keyEquivalent:@""]);
  [syncMenuItem setTag:IDC_SHOW_SYNC_SETUP];

  // Now sign in.
  std::string username = "foo@example.com";
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
  signin->SetAuthenticatedAccountInfo(username, username);
  ProfileSyncService* sync = ProfileSyncServiceFactory::GetForProfile(profile_);
  sync->SetSyncSetupCompleted();
  // Force an auth error.
  FakeAuthStatusProvider provider(
      SigninErrorControllerFactory::GetForProfile(profile_));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  provider.SetAuthError("user@gmail.com", error);
  [AppController updateSigninItem:syncMenuItem
                       shouldShow:YES
                   currentProfile:profile_];
  NSString* authError =
      l10n_util::GetNSStringWithFixup(IDS_SYNC_SIGN_IN_ERROR_WRENCH_MENU_ITEM);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:authError]);
  EXPECT_FALSE([syncMenuItem isHidden]);
}

// If there's a separator after the signin menu item, make sure it is hidden/
// shown when the signin menu item is.
TEST_F(AppControllerTest, TestSigninMenuItemWithSeparator) {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* signinMenuItem = [menu addItemWithTitle:@""
                                               action:@selector(commandDispatch)
                                        keyEquivalent:@""];
  [signinMenuItem setTag:IDC_SHOW_SYNC_SETUP];
  NSMenuItem* followingSeparator = [NSMenuItem separatorItem];
  [menu addItem:followingSeparator];
  [signinMenuItem setHidden:NO];
  [followingSeparator setHidden:NO];

  [AppController updateSigninItem:signinMenuItem
                       shouldShow:NO
                   currentProfile:profile_];

  EXPECT_FALSE([followingSeparator isEnabled]);
  EXPECT_TRUE([signinMenuItem isHidden]);
  EXPECT_TRUE([followingSeparator isHidden]);

  [AppController updateSigninItem:signinMenuItem
                       shouldShow:YES
                   currentProfile:profile_];

  EXPECT_FALSE([followingSeparator isEnabled]);
  EXPECT_FALSE([signinMenuItem isHidden]);
  EXPECT_FALSE([followingSeparator isHidden]);
}

// If there's a non-separator item after the signin menu item, it should not
// change state when the signin menu item is hidden/shown.
TEST_F(AppControllerTest, TestSigninMenuItemWithNonSeparator) {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* signinMenuItem = [menu addItemWithTitle:@""
                                               action:@selector(commandDispatch)
                                        keyEquivalent:@""];
  [signinMenuItem setTag:IDC_SHOW_SYNC_SETUP];
  NSMenuItem* followingNonSeparator =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];
  [signinMenuItem setHidden:NO];
  [followingNonSeparator setHidden:NO];

  [AppController updateSigninItem:signinMenuItem
                       shouldShow:NO
                   currentProfile:profile_];

  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_TRUE([signinMenuItem isHidden]);
  EXPECT_FALSE([followingNonSeparator isHidden]);

  [followingNonSeparator setHidden:YES];
  [AppController updateSigninItem:signinMenuItem
                       shouldShow:YES
                   currentProfile:profile_];

  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_FALSE([signinMenuItem isHidden]);
  EXPECT_TRUE([followingNonSeparator isHidden]);
}
