// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_client.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace {

constexpr char kTestUserName1[] = "test1@test.com";
constexpr char kTestUser1GaiaId[] = "1111111111";
constexpr char kTestUserName2[] = "test2@test.com";
constexpr char kTestUser2GaiaId[] = "2222222222";

void CreateAndStartUserSession(const AccountId& account_id) {
  using chromeos::ProfileHelper;
  using session_manager::SessionManager;

  user_manager::known_user::SetProfileRequiresPolicy(
      account_id,
      user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired);
  const std::string user_id_hash =
      ProfileHelper::GetUserIdHashByUserIdForTesting(account_id.GetUserEmail());
  SessionManager::Get()->CreateSession(account_id, user_id_hash, false);
  ProfileHelper::GetProfileByUserIdHashForTest(user_id_hash);
  SessionManager::Get()->SessionStarted();
}

// Give the underlying function a clearer name.
Browser* GetLastActiveBrowser() {
  return chrome::FindLastActive();
}

}  // namespace

using ChromeNewWindowClientBrowserTest = InProcessBrowserTest;

// Tests that when we open a new window by pressing 'Ctrl-N', we should use the
// current active window's profile to determine on which profile's desktop we
// should open a new window.
IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest,
                       NewWindowForActiveWindowProfileTest) {
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName1, kTestUser1GaiaId));
  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  Browser* browser1 = CreateBrowser(profile1);
  // The newly created window should be created for the current active profile.
  ChromeNewWindowClient::Get()->NewWindow(false /* incognito */);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Login another user and make sure the current active user changes.
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName2, kTestUser2GaiaId));
  Profile* profile2 = ProfileManager::GetActiveUserProfile();
  EXPECT_NE(profile1, profile2);

  Browser* browser2 = CreateBrowser(profile2);
  // The newly created window should be created for the current active window's
  // profile, which is |profile2|.
  ChromeNewWindowClient::Get()->NewWindow(false /* incognito */);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile2);

  // After activating |browser1|, the newly created window should be created
  // against |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(false /* incognito */);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Test for incognito windows.
  // The newly created incoginito window should be created against the current
  // active |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(true /* incognito */);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile1);

  // The newly created incoginito window should be created against the current
  // active |browser2|'s profile.
  browser2->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(true /* incognito */);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile2);
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, IncognitoDisabled) {
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName1, kTestUser2GaiaId));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Disabling incognito mode disables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(profile->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  ChromeNewWindowClient::Get()->NewWindow(true /* incognito */);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Enabling incognito mode enables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(profile->GetPrefs(),
                                      IncognitoModePrefs::ENABLED);
  ChromeNewWindowClient::Get()->NewWindow(true /* incognito */);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(Profile::INCOGNITO_PROFILE,
            GetLastActiveBrowser()->profile()->GetProfileType());
}
