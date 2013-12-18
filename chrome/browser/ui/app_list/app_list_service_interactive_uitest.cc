// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"

// Interactive UI Test for AppListService that runs on all platforms supporting
// app_list. Interactive because the app list uses focus changes to dismiss
// itself, which will cause tests that check the visibility to fail flakily.
class AppListServiceInteractiveTest : public InProcessBrowserTest {
 public:
  AppListServiceInteractiveTest()
    : profile2_(NULL) {}

  void InitSecondProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath temp_profile_dir =
        profile_manager->user_data_dir().AppendASCII("Profile 1");
    profile_manager->CreateProfileAsync(
        temp_profile_dir,
        base::Bind(&AppListServiceInteractiveTest::OnProfileCreated,
                   this),
        base::string16(), base::string16(), std::string());
    content::RunMessageLoop();  // Will stop in OnProfileCreated().
  }

  void OnProfileCreated(Profile* profile, Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      profile2_ = profile;
      base::MessageLoop::current()->Quit();
    }
  }

 protected:
  Profile* profile2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceInteractiveTest);
};

// ChromeOS does not support ShowForProfile(), or profile switching, because
// the browser instance is only ever associated with a single profile.
#if defined(OS_CHROMEOS)
#define MAYBE_ShowAndDismiss DISABLED_ShowAndDismiss
#define MAYBE_SwitchAppListProfiles DISABLED_SwitchAppListProfiles
#define MAYBE_SwitchAppListProfilesDuringSearch \
    DISABLED_SwitchAppListProfilesDuringSearch
#else
#define MAYBE_ShowAndDismiss ShowAndDismiss
#define MAYBE_SwitchAppListProfiles SwitchAppListProfiles
#define MAYBE_SwitchAppListProfilesDuringSearch \
    SwitchAppListProfilesDuringSearch
#endif

// Show the app list, then dismiss it.
IN_PROC_BROWSER_TEST_F(AppListServiceInteractiveTest, MAYBE_ShowAndDismiss) {
  AppListService* service = test::GetAppListService();
  ASSERT_FALSE(service->IsAppListVisible());
  service->ShowForProfile(browser()->profile());
  ASSERT_TRUE(service->IsAppListVisible());
  service->DismissAppList();
  ASSERT_FALSE(service->IsAppListVisible());
}

// Switch profiles on the app list while it is showing.
IN_PROC_BROWSER_TEST_F(AppListServiceInteractiveTest,
                       MAYBE_SwitchAppListProfiles) {
  InitSecondProfile();
  test::SigninProfile(browser()->profile());
  test::SigninProfile(profile2_);

  AppListService* service = test::GetAppListService();
  ASSERT_TRUE(service);

  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);

  // Open the app list with the browser's profile.
  ASSERT_FALSE(service->IsAppListVisible());
  controller->ShowForProfileByPath(browser()->profile()->GetPath());
  app_list::AppListModel* model = test::GetAppListModel(service);
  ASSERT_TRUE(model);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(browser()->profile(), service->GetCurrentAppListProfile());

  // Open the app list with the second profile.
  controller->ShowForProfileByPath(profile2_->GetPath());
  model = test::GetAppListModel(service);
  ASSERT_TRUE(model);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(profile2_, service->GetCurrentAppListProfile());

  controller->DismissView();
}

// Test switching app list profiles while search results are visibile.
IN_PROC_BROWSER_TEST_F(AppListServiceInteractiveTest,
                       MAYBE_SwitchAppListProfilesDuringSearch) {
  InitSecondProfile();
  test::SigninProfile(browser()->profile());
  test::SigninProfile(profile2_);

  AppListService* service = test::GetAppListService();
  ASSERT_TRUE(service);

  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);

  // Set a search with original profile.
  controller->ShowForProfileByPath(browser()->profile()->GetPath());
  app_list::AppListModel* model = test::GetAppListModel(service);
  ASSERT_TRUE(model);

  model->search_box()->SetText(ASCIIToUTF16("minimal"));
  base::RunLoop().RunUntilIdle();

  // Switch to the second profile.
  controller->ShowForProfileByPath(profile2_->GetPath());
  model = test::GetAppListModel(service);
  ASSERT_TRUE(model);
  base::RunLoop().RunUntilIdle();

  // Ensure the search box is empty.
  ASSERT_TRUE(model->search_box()->text().empty());
  ASSERT_EQ(profile2_, service->GetCurrentAppListProfile());

  controller->DismissView();
  ASSERT_FALSE(service->IsAppListVisible());
}

// Interactive UI test that adds the --show-app-list command line switch.
class ShowAppListInteractiveTest : public InProcessBrowserTest {
 public:
  ShowAppListInteractiveTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kShowAppList);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShowAppListInteractiveTest);
};

// Test showing the app list using the command line switch.
IN_PROC_BROWSER_TEST_F(ShowAppListInteractiveTest, ShowAppListFlag) {
  AppListService* service = test::GetAppListService();
  // The app list should already be shown because we passed
  // switches::kShowAppList.
  ASSERT_TRUE(service->IsAppListVisible());

  // Create a browser to prevent shutdown when we dismiss the app list.  We
  // need to do this because switches::kShowAppList suppresses the creation of
  // any browsers.
  CreateBrowser(service->GetCurrentAppListProfile());
  service->DismissAppList();
}
