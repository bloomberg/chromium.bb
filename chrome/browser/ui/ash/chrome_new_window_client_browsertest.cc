// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/new_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace {

const char kTestUserName1[] = "test1@test.com";
const char kTestUserName2[] = "test2@test.com";

void CreateAndStartUserSession(const AccountId& account_id) {
  using chromeos::ProfileHelper;
  using session_manager::SessionManager;

  const std::string user_id_hash =
      ProfileHelper::GetUserIdHashByUserIdForTesting(account_id.GetUserEmail());
  SessionManager::Get()->CreateSession(account_id, user_id_hash);
  ProfileHelper::GetProfileByUserIdHashForTest(user_id_hash);
  SessionManager::Get()->SessionStarted();
}

}  // namespace

using ChromeNewWindowClientBrowserTest = InProcessBrowserTest;

// Tests that when we open a new window by pressing 'Ctrl-N', we should use the
// current active window's profile to determine on which profile's desktop we
// should open a new window.
IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest,
                       NewWindowForActiveWindowProfileTest) {
  CreateAndStartUserSession(AccountId::FromUserEmail(kTestUserName1));
  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  Browser* browser1 = CreateBrowser(profile1);
  // The newly created window should be created for the current active profile.
  ash::Shell::Get()->new_window_controller()->NewWindow(false);
  EXPECT_EQ(
      chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow())->profile(),
      profile1);

  // Login another user and make sure the current active user changes.
  CreateAndStartUserSession(AccountId::FromUserEmail(kTestUserName2));
  Profile* profile2 = ProfileManager::GetActiveUserProfile();
  EXPECT_NE(profile1, profile2);

  Browser* browser2 = CreateBrowser(profile2);
  // The newly created window should be created for the current active window's
  // profile, which is |profile2|.
  ash::Shell::Get()->new_window_controller()->NewWindow(false);
  EXPECT_EQ(
      chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow())->profile(),
      profile2);

  // After activating |browser1|, the newly created window should be created
  // against |browser1|'s profile.
  browser1->window()->Show();
  ash::Shell::Get()->new_window_controller()->NewWindow(false);
  EXPECT_EQ(
      chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow())->profile(),
      profile1);

  // Test for incognito windows.
  // The newly created incoginito window should be created against the current
  // active |browser1|'s profile.
  browser1->window()->Show();
  ash::Shell::Get()->new_window_controller()->NewWindow(true);
  EXPECT_EQ(chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow())
                ->profile()
                ->GetOriginalProfile(),
            profile1);

  // The newly created incoginito window should be created against the current
  // active |browser2|'s profile.
  browser2->window()->Show();
  ash::Shell::Get()->new_window_controller()->NewWindow(true);
  EXPECT_EQ(chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow())
                ->profile()
                ->GetOriginalProfile(),
            profile2);
}
