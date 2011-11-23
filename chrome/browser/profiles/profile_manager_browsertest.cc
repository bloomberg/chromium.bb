// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"

// This file contains tests for the ProfileManager that require a heavyweight
// InProcessBrowserTest.  These include tests involving profile deletion.

// TODO(jeremy): crbug.com/103355 - These tests should be enabled on all
// platforms.
#if defined(OS_MACOSX)
class ProfileManagerBrowserTest : public InProcessBrowserTest {
 protected:
  // An observer that returns back to test code after a new profile
  // is initialized.
  class BlockOnProfileInitObserver : public ProfileManagerObserver {
   public:
    void OnProfileCreated(Profile* profile, Status status) {
      if (status == ProfileManagerObserver::STATUS_INITIALIZED) {
        MessageLoop::current()->Quit();
      }
    }
  };
};

// Delete single profile and make sure a new one is created.
IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, DeleteSingletonProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  // We should start out with 1 profile.
  ASSERT_EQ(cache.GetNumberOfProfiles(), 1U);

  // Delete singleton profile.
  FilePath singleton_profile_path = cache.GetPathOfProfileAtIndex(0);
  EXPECT_FALSE(singleton_profile_path.empty());
  profile_manager->ScheduleProfileForDeletion(singleton_profile_path);

  // Spin things till profile is actually deleted.
  ui_test_utils::RunAllPendingInMessageLoop();

  // Make sure a new profile was created automatically.
  EXPECT_EQ(cache.GetNumberOfProfiles(), 1U);
  FilePath new_profile_path = cache.GetPathOfProfileAtIndex(0);
  EXPECT_NE(new_profile_path, singleton_profile_path);

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_profile_path, last_used->GetPath());
}

// Delete all profiles in a multi profile setup and make sure a new one is
// created.

#if defined(OS_MACOSX)
// See crbug.com/104851
#define MAYBE_DeleteAllProfiles FLAKY_DeleteAllProfiles
#else
#define MAYBE_DeleteAllProfiles DeleteAllProfiles
#endif

IN_PROC_BROWSER_TEST_F(ProfileManagerBrowserTest, MAYBE_DeleteAllProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  // Create an additional profile.
  BlockOnProfileInitObserver observer;
  FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  profile_manager->CreateProfileAsync(new_path, &observer);

  // Spin to allow profile creation to take place, loop is terminated
  // by BlockOnProfileInitObserver when the profile is created.
  ui_test_utils::RunMessageLoop();

  ASSERT_EQ(cache.GetNumberOfProfiles(), 2U);

  // Delete all profiles.
  FilePath profile_path1 = cache.GetPathOfProfileAtIndex(0);
  FilePath profile_path2 = cache.GetPathOfProfileAtIndex(1);
  EXPECT_FALSE(profile_path1.empty());
  EXPECT_FALSE(profile_path2.empty());
  profile_manager->ScheduleProfileForDeletion(profile_path1);
  profile_manager->ScheduleProfileForDeletion(profile_path2);

  // Spin things so deletion can take place.
  ui_test_utils::RunAllPendingInMessageLoop();

  // Make sure a new profile was created automatically.
  EXPECT_EQ(cache.GetNumberOfProfiles(), 1U);
  FilePath new_profile_path = cache.GetPathOfProfileAtIndex(0);
  EXPECT_NE(new_profile_path, profile_path1);
  EXPECT_NE(new_profile_path, profile_path2);

  // Make sure that last used profile preference is set correctly.
  Profile* last_used = ProfileManager::GetLastUsedProfile();
  EXPECT_EQ(new_profile_path, last_used->GetPath());
}
#endif // OS_MACOSX
