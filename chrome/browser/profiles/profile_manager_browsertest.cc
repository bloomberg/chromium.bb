// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"

namespace {

// An observer that returns back to test code after a new profile is
// initialized.
void OnUnblockOnProfileCreation(Profile* profile,
                                Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    MessageLoop::current()->Quit();
}

} // namespace

// This file contains tests for the ProfileManager that require a heavyweight
// InProcessBrowserTest.  These include tests involving profile deletion.

// TODO(jeremy): crbug.com/103355 - These tests should be enabled on all
// platforms.
#if defined(OS_MACOSX)
class ProfileManagerBrowserTest : public InProcessBrowserTest {
};

// Delete single profile and make sure a new one is created.
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, DeleteSingletonProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  // We should start out with 1 profile.
  ASSERT_EQ(cache.GetNumberOfProfiles(), 1U);

  // Delete singleton profile.
  base::FilePath singleton_profile_path = cache.GetPathOfProfileAtIndex(0);
  EXPECT_FALSE(singleton_profile_path.empty());
  profile_manager->ScheduleProfileForDeletion(singleton_profile_path,
                                              chrome::HOST_DESKTOP_TYPE_NATIVE);

  // Spin things till profile is actually deleted.
  content::RunAllPendingInMessageLoop();

  // Make sure a new profile was created automatically.
  EXPECT_EQ(cache.GetNumberOfProfiles(), 1U);
  base::FilePath new_profile_path = cache.GetPathOfProfileAtIndex(0);
  EXPECT_NE(new_profile_path, singleton_profile_path);

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_profile_path, last_used->GetPath());
}

// Delete all profiles in a multi profile setup and make sure a new one is
// created.

#if defined(OS_MACOSX)
// Crashes/CHECKs. See crbug.com/104851
#define MAYBE_DeleteAllProfiles DISABLED_DeleteAllProfiles
#else
#define MAYBE_DeleteAllProfiles DeleteAllProfiles
#endif

IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, MAYBE_DeleteAllProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  // Create an additional profile.
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  profile_manager->CreateProfileAsync(new_path,
                                      base::Bind(&OnUnblockOnProfileCreation),
                                      string16(), string16(), false);

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  content::RunMessageLoop();

  ASSERT_EQ(cache.GetNumberOfProfiles(), 2U);

  // Delete all profiles.
  base::FilePath profile_path1 = cache.GetPathOfProfileAtIndex(0);
  base::FilePath profile_path2 = cache.GetPathOfProfileAtIndex(1);
  EXPECT_FALSE(profile_path1.empty());
  EXPECT_FALSE(profile_path2.empty());
  profile_manager->ScheduleProfileForDeletion(profile_path1,
                                              chrome::HOST_DESKTOP_TYPE_NATIVE);
  profile_manager->ScheduleProfileForDeletion(profile_path2,
                                              chrome::HOST_DESKTOP_TYPE_NATIVE);

  // Spin things so deletion can take place.
  content::RunAllPendingInMessageLoop();

  // Make sure a new profile was created automatically.
  EXPECT_EQ(cache.GetNumberOfProfiles(), 1U);
  base::FilePath new_profile_path = cache.GetPathOfProfileAtIndex(0);
  EXPECT_NE(new_profile_path, profile_path1);
  EXPECT_NE(new_profile_path, profile_path2);

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_profile_path, last_used->GetPath());
}
#endif // OS_MACOSX
