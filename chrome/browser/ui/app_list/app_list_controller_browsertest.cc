// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

// Browser Test for AppListController that runs on all platforms supporting
// app_list.
class AppListControllerBrowserTest : public InProcessBrowserTest {
 public:
  AppListControllerBrowserTest()
    : profile2_(NULL) {}

  void OnProfileCreated(Profile* profile, Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      profile2_ = profile;
      MessageLoop::current()->Quit();
    }
  }

 protected:
  base::ScopedTempDir temp_profile_dir_;
  Profile* profile2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerBrowserTest);
};

#if !defined(OS_CHROMEOS) && !defined(USE_AURA)
// Show the app list, then dismiss it.
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, ShowAndDismiss) {
  AppListService* service = AppListService::Get();
  ASSERT_FALSE(service->IsAppListVisible());
  service->ShowAppList(browser()->profile());
  ASSERT_TRUE(service->IsAppListVisible());
  service->DismissAppList();
  ASSERT_FALSE(service->IsAppListVisible());
}

// TODO(tapted): Enable this when profile switching code has been moved up the
// app list controller hierarchy.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, SwitchAppListProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  profile_manager->CreateProfileAsync(
      temp_profile_dir_.path(),
      base::Bind(&AppListControllerBrowserTest::OnProfileCreated,
                 this),
      string16(), string16(), false);
  content::RunMessageLoop();  // Will stop in OnProfileCreated().

  AppListService* service = AppListService::Get();
  ASSERT_FALSE(service->IsAppListVisible());
  service->ShowAppList(browser()->profile());
  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(browser()->profile(), service->GetCurrentAppListProfile());
  service->ShowAppList(profile2_);
  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(profile2_, service->GetCurrentAppListProfile());
  service->DismissAppList();
  ASSERT_FALSE(service->IsAppListVisible());
}

class ShowAppListBrowserTest : public InProcessBrowserTest {
 public:
  ShowAppListBrowserTest() {}

  void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kShowAppList);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShowAppListBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ShowAppListBrowserTest, ShowAppListFlag) {
  AppListService* service = AppListService::Get();
  // The app list should already be shown because we passed
  // switches::kShowAppList.
  ASSERT_TRUE(service->IsAppListVisible());

  // Create a browser to prevent shutdown when we dismiss the app list.  We
  // need to do this because switches::kShowAppList suppresses the creation of
  // any browsers.
  CreateBrowser(service->GetCurrentAppListProfile());
  service->DismissAppList();
}
#endif  // !defined(OS_MACOSX)
#endif  // !defined(OS_CHROMEOS) && !defined(USE_AURA)
