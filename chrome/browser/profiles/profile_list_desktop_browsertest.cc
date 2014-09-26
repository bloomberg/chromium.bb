// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/user_manager.h"
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

}  // namespace

class ProfileListDesktopBrowserTest : public InProcessBrowserTest {
 public:
  ProfileListDesktopBrowserTest() {}

  scoped_ptr<AvatarMenu> CreateAvatarMenu(ProfileInfoCache* cache) {
    return scoped_ptr<AvatarMenu>(new AvatarMenu(cache, NULL, browser()));
  }

 private:
  scoped_ptr<AvatarMenu> avatar_menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListDesktopBrowserTest);
};

#if defined(OS_WIN)
// SignOut is flaky. So far only observed on Windows. crbug.com/357329.
#define MAYBE_SignOut DISABLED_SignOut
#elif defined(OS_CHROMEOS)
// This test doesn't make sense for Chrome OS since it has a different
// multi-profiles menu in the system tray instead.
#define MAYBE_SignOut DISABLED_SignOut
#else
#define MAYBE_SignOut SignOut
#endif
IN_PROC_BROWSER_TEST_F(ProfileListDesktopBrowserTest, MAYBE_SignOut) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* current_profile = browser()->profile();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(current_profile->GetPath());

  scoped_ptr<AvatarMenu> menu = CreateAvatarMenu(&cache);
  menu->RebuildMenu();

  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::GetActiveDesktop());
  EXPECT_EQ(1U, browser_list->size());
  content::WindowedNotificationObserver window_close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));

  EXPECT_FALSE(cache.ProfileIsSigninRequiredAtIndex(index));
  profiles::LockProfile(current_profile);
  window_close_observer.Wait();  // rely on test time-out for failure indication

  EXPECT_TRUE(cache.ProfileIsSigninRequiredAtIndex(index));
  EXPECT_EQ(0U, browser_list->size());

  // Signing out brings up the User Manager which we should close before exit.
  UserManager::Hide();
}

#if defined(OS_CHROMEOS)
// This test doesn't make sense for Chrome OS since it has a different
// multi-profiles menu in the system tray instead.
#define MAYBE_SwitchToProfile DISABLED_SwitchToProfile
#else
#define MAYBE_SwitchToProfile SwitchToProfile
#endif
IN_PROC_BROWSER_TEST_F(ProfileListDesktopBrowserTest, MAYBE_SwitchToProfile) {
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
                                      base::string16(), base::string16(),
                                      std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  content::RunMessageLoop();
  ASSERT_EQ(cache.GetNumberOfProfiles(), 2U);

  scoped_ptr<AvatarMenu> menu = CreateAvatarMenu(&cache);
  menu->RebuildMenu();
  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::GetActiveDesktop());
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the first profile.
  menu->SwitchToProfile(cache.GetIndexOfProfileWithPath(path_profile1),
                        false, ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(1U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());

  // Open a browser window for the second profile.
  menu->SwitchToProfile(cache.GetIndexOfProfileWithPath(path_profile2),
                        false, ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(2U, browser_list->size());

  // Switch to the first profile without opening a new window.
  menu->SwitchToProfile(cache.GetIndexOfProfileWithPath(path_profile1),
                        false, ProfileMetrics::SWITCH_PROFILE_ICON);
  EXPECT_EQ(2U, browser_list->size());
  EXPECT_EQ(path_profile1, browser_list->get(0)->profile()->GetPath());
  EXPECT_EQ(path_profile2, browser_list->get(1)->profile()->GetPath());
}
