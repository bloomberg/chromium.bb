// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user_names.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/models/list_model_observer.h"

// Browser Test for AppListController that runs on all platforms supporting
// app_list.
typedef InProcessBrowserTest AppListControllerBrowserTest;

// Test the CreateNewWindow function of the controller delegate.
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, CreateNewWindow) {
  AppListService* service = AppListService::Get();
  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);

  EXPECT_EQ(1U, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(0U, chrome::GetBrowserCount(
                    browser()->profile()->GetOffTheRecordProfile()));

  controller->CreateNewWindow(browser()->profile(), false);
  EXPECT_EQ(2U, chrome::GetBrowserCount(browser()->profile()));

  controller->CreateNewWindow(browser()->profile(), true);
  EXPECT_EQ(1U, chrome::GetBrowserCount(
                    browser()->profile()->GetOffTheRecordProfile()));
}

// Browser Test for AppListController that observes search result changes.
class AppListControllerSearchResultsBrowserTest
    : public ExtensionBrowserTest,
      public ui::ListModelObserver {
 public:
  AppListControllerSearchResultsBrowserTest()
      : observed_result_(nullptr), observed_results_list_(nullptr) {}

  void WatchResultsLookingForItem(
      app_list::SearchModel::SearchResults* search_results,
      const std::string& extension_name) {
    EXPECT_FALSE(observed_results_list_);
    observed_results_list_ = search_results;
    observed_results_list_->AddObserver(this);
    item_to_observe_ = base::ASCIIToUTF16(extension_name);
  }

  void StopWatchingResults() {
    EXPECT_TRUE(observed_results_list_);
    observed_results_list_->RemoveObserver(this);
  }

 protected:
  void AttemptToLocateItem() {
    observed_result_ = nullptr;

    for (size_t i = 0; i < observed_results_list_->item_count(); ++i) {
      if (observed_results_list_->GetItemAt(i)->title() != item_to_observe_)
        continue;

      // Ensure there is at most one.
      EXPECT_FALSE(observed_result_);
      observed_result_ = observed_results_list_->GetItemAt(i);
    }
  }

  // Overridden from ui::ListModelObserver:
  void ListItemsAdded(size_t start, size_t count) override {
    AttemptToLocateItem();
  }
  void ListItemsRemoved(size_t start, size_t count) override {
    AttemptToLocateItem();
  }
  void ListItemMoved(size_t index, size_t target_index) override {}
  void ListItemsChanged(size_t start, size_t count) override {}

  app_list::SearchResult* observed_result_;

 private:
  base::string16 item_to_observe_;
  app_list::SearchModel::SearchResults* observed_results_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerSearchResultsBrowserTest);
};

// Test showing search results, and uninstalling one of them while displayed.
IN_PROC_BROWSER_TEST_F(AppListControllerSearchResultsBrowserTest,
                       UninstallSearchResult) {
  base::FilePath test_extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
  test_extension_path = test_extension_path.AppendASCII("extensions")
      .AppendASCII("platform_apps")
      .AppendASCII("minimal");
  const extensions::Extension* extension =
      InstallExtension(test_extension_path,
                       1 /* expected_change: new install */);
  ASSERT_TRUE(extension);

  AppListService* service = AppListService::Get();
  ASSERT_TRUE(service);
  service->ShowForProfile(browser()->profile());

  app_list::SearchModel* model = test::GetSearchModel(service);
  ASSERT_TRUE(model);
  WatchResultsLookingForItem(model->results(), extension->name());

  // Ensure a search finds the extension.
  EXPECT_FALSE(observed_result_);
  model->search_box()->Update(base::ASCIIToUTF16("minimal"),
                              true /* initiated_by_user */);
  EXPECT_TRUE(observed_result_);

  // Ensure the UI is updated. This is via PostTask in views.
  base::RunLoop().RunUntilIdle();

  // Now uninstall and ensure this browser test observes it.
  UninstallExtension(extension->id());

  // Allow async AppSearchProvider::UpdateResults to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(observed_result_);
  StopWatchingResults();
  service->DismissAppList();
}

class AppListControllerGuestModeBrowserTest : public InProcessBrowserTest {
 public:
  AppListControllerGuestModeBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerGuestModeBrowserTest);
};

void AppListControllerGuestModeBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                  user_manager::kGuestUserName);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                  TestingProfile::kTestUserProfileDir);
  command_line->AppendSwitch(switches::kIncognito);
}

// Test creating the initial app list in guest mode.
IN_PROC_BROWSER_TEST_F(AppListControllerGuestModeBrowserTest, Incognito) {
  AppListService* service = AppListService::Get();
  EXPECT_TRUE(service->GetCurrentAppListProfile());

  service->ShowForProfile(browser()->profile());
  EXPECT_EQ(browser()->profile(), service->GetCurrentAppListProfile());
}
