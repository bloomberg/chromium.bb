// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/platform_test.h"

class AppControllerTest : public PlatformTest {
 protected:
  AppControllerTest() {}

  virtual void TearDown() {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
    base::RunLoop().RunUntilIdle();
  }

  content::TestBrowserThreadBundle thread_bundle_;
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
  TestingProfileManager manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(manager.SetUp());

  // Create two profiles.
  base::FilePath dest_path1 =
      manager.CreateTestingProfile("New Profile 1")->GetPath();
  base::FilePath dest_path2 =
      manager.CreateTestingProfile("New Profile 2")->GetPath();
  ASSERT_EQ(2U, manager.profile_manager()->GetNumberOfProfiles());
  ASSERT_EQ(2U, manager.profile_manager()->GetLoadedProfiles().size());

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path1.BaseName().MaybeAsASCII());

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);

  // Delete the active profile.
  manager.profile_manager()->ScheduleProfileForDeletion(
      dest_path1, ProfileManager::CreateCallback());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dest_path2, [ac lastProfile]->GetPath());
}
