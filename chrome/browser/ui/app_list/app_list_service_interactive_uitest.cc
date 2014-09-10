// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
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

  void InitSecondProfile() { profile2_ = test::CreateSecondProfileAsync(); }

 protected:
  Profile* profile2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceInteractiveTest);
};

// ChromeOS does not support ShowForProfile(), or profile switching within the
// app list. Profile switching on CrOS goes through a different code path.
#if defined(OS_CHROMEOS)
#define MAYBE_ShowAndDismiss DISABLED_ShowAndDismiss
#define MAYBE_SwitchAppListProfiles DISABLED_SwitchAppListProfiles
#define MAYBE_SwitchAppListProfilesDuringSearch \
    DISABLED_SwitchAppListProfilesDuringSearch
#define MAYBE_ShowAppListNonDefaultProfile \
    DISABLED_ShowAppListNonDefaultProfile
#define MAYBE_DeleteShowingAppList DISABLED_DeleteShowingAppList
#else
#define MAYBE_ShowAndDismiss ShowAndDismiss
#define MAYBE_SwitchAppListProfiles SwitchAppListProfiles
#define MAYBE_SwitchAppListProfilesDuringSearch \
    SwitchAppListProfilesDuringSearch
#define MAYBE_ShowAppListNonDefaultProfile ShowAppListNonDefaultProfile
#define MAYBE_DeleteShowingAppList DeleteShowingAppList
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

  AppListService* service = test::GetAppListService();
  ASSERT_TRUE(service);

  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);

  // Set a search with original profile.
  controller->ShowForProfileByPath(browser()->profile()->GetPath());
  app_list::AppListModel* model = test::GetAppListModel(service);
  ASSERT_TRUE(model);

  model->search_box()->SetText(base::ASCIIToUTF16("minimal"));
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
#if defined(OS_CHROMEOS)
// http://crbug.com/396499
#define MAYBE_ShowAppListFlag DISABLED_ShowAppListFlag
#else
#define MAYBE_ShowAppListFlag ShowAppListFlag
#endif
IN_PROC_BROWSER_TEST_F(ShowAppListInteractiveTest, MAYBE_ShowAppListFlag) {
  AppListService* service = test::GetAppListService();
  // The app list should already be shown because we passed
  // switches::kShowAppList.
  EXPECT_TRUE(service->IsAppListVisible());

  // Create a browser to prevent shutdown when we dismiss the app list.  We
  // need to do this because switches::kShowAppList suppresses the creation of
  // any browsers.
  Profile* profile = service->GetCurrentAppListProfile();
  CreateBrowser(profile);

  service->DismissAppList();
  EXPECT_FALSE(service->IsAppListVisible());

  // With Chrome still running, test receiving a second --show-app-list request
  // via the process singleton. ChromeOS has no process singleton so exclude it.
#if !defined(OS_CHROMEOS)
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kShowAppList);
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, base::FilePath(), profile->GetPath());

  EXPECT_TRUE(service->IsAppListVisible());
  service->DismissAppList();
  EXPECT_FALSE(service->IsAppListVisible());
#endif
}

// Interactive UI test that creates a non-default profile and configures it for
// the --show-app-list flag.
class ShowAppListNonDefaultInteractiveTest : public ShowAppListInteractiveTest {
 public:
  ShowAppListNonDefaultInteractiveTest()
      : second_profile_name_(FILE_PATH_LITERAL("Profile 1")) {
  }

  virtual bool SetUpUserDataDirectory() OVERRIDE {
    // Create a temp dir for "Profile 1" and seed the user data dir with a Local
    // State file configuring the app list to use it.
    base::FilePath user_data_dir;
    CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    base::FilePath profile_path = user_data_dir.Append(second_profile_name_);
    CHECK(second_profile_temp_dir_.Set(profile_path));

    base::FilePath local_pref_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    base::DictionaryValue dict;
    dict.SetString(prefs::kAppListProfile,
                   second_profile_name_.MaybeAsASCII());
    CHECK(JSONFileValueSerializer(local_pref_path).Serialize(dict));

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

 protected:
  const base::FilePath second_profile_name_;
  base::ScopedTempDir second_profile_temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShowAppListNonDefaultInteractiveTest);
};

// Test showing the app list for a profile that doesn't match the browser
// profile.
IN_PROC_BROWSER_TEST_F(ShowAppListNonDefaultInteractiveTest,
                       MAYBE_ShowAppListNonDefaultProfile) {
  AppListService* service = test::GetAppListService();
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(second_profile_name_.value(),
            service->GetCurrentAppListProfile()->GetPath().BaseName().value());

  // Check that the default profile hasn't been loaded.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(1u, profile_manager->GetNumberOfProfiles());

  // Create a browser for the Default profile. This stops MaybeTeminate being
  // called when the app list window is dismissed. Use the last used browser
  // profile to verify that it is different and causes ProfileManager to load a
  // new profile.
  CreateBrowser(profile_manager->GetLastUsedProfile());
  EXPECT_EQ(2u, profile_manager->GetNumberOfProfiles());

  service->DismissAppList();
}

// Test showing the app list for a profile then deleting that profile while the
// app list is visible.
IN_PROC_BROWSER_TEST_F(ShowAppListNonDefaultInteractiveTest,
                       MAYBE_DeleteShowingAppList) {
  AppListService* service = test::GetAppListService();
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(second_profile_name_.value(),
            service->GetCurrentAppListProfile()->GetPath().BaseName().value());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create a browser for the Default profile.
  CreateBrowser(profile_manager->GetLastUsedProfile());

  // Delete the profile being used by the app list.
  profile_manager->ScheduleProfileForDeletion(
      service->GetCurrentAppListProfile()->GetPath(),
      ProfileManager::CreateCallback());

  // App Launcher should get closed immediately and nothing should explode.
  EXPECT_FALSE(service->IsAppListVisible());
}
