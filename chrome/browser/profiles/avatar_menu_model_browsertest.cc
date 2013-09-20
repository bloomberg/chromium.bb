// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_utils.h"

namespace {

// An observer that returns back to test code after a new profile is
// initialized.
void OnUnblockOnProfileCreation(Profile* profile,
                                Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    base::MessageLoop::current()->Quit();
}

typedef InProcessBrowserTest AvatarMenuModelTest;

IN_PROC_BROWSER_TEST_F(AvatarMenuModelTest, SignOut) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* current_profile = browser()->profile();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(current_profile->GetPath());

  AvatarMenuModel model(&cache, NULL, browser());

  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::GetActiveDesktop());
  EXPECT_EQ(1U, browser_list->size());
  content::WindowedNotificationObserver window_close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));

  EXPECT_FALSE(cache.ProfileIsSigninRequiredAtIndex(index));
  model.SetLogoutURL("about:blank");
  model.BeginSignOut();
  EXPECT_TRUE(cache.ProfileIsSigninRequiredAtIndex(index));

  window_close_observer.Wait();  // rely on test time-out for failure indication
  EXPECT_EQ(0U, browser_list->size());
}

IN_PROC_BROWSER_TEST_F(AvatarMenuModelTest, SwitchToProfile) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* current_profile = browser()->profile();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  base::FilePath path_profile1 = current_profile->GetPath();
  base::FilePath user_dir = cache.GetUserDataDir();

  // Create an additional profile.
  base::FilePath path_profile2 = user_dir.Append(
      FILE_PATH_LITERAL("New Profile 2"));
  profile_manager->CreateProfileAsync(path_profile2,
                                      base::Bind(&OnUnblockOnProfileCreation),
                                      string16(), string16(), std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  content::RunMessageLoop();
  ASSERT_EQ(cache.GetNumberOfProfiles(), 2U);

  AvatarMenuModel model(&cache, NULL, browser());
  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::GetActiveDesktop());
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the first profile.
  model.SwitchToProfile(cache.GetIndexOfProfileWithPath(path_profile1), false);
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the second profile.
  model.SwitchToProfile(cache.GetIndexOfProfileWithPath(path_profile2), false);
  EXPECT_EQ(2U, browser_list->size());

  // Switch to the first profile without opening a new window.
  model.SwitchToProfile(cache.GetIndexOfProfileWithPath(path_profile1), false);
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());

}

}  // namespace
