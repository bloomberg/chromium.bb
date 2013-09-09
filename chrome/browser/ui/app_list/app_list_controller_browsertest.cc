// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/test/app_list_service_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/search_result_observer.h"
#include "ui/base/models/list_model_observer.h"

namespace {

// Browser Test for AppListController that runs on all platforms supporting
// app_list.
class AppListControllerBrowserTest : public InProcessBrowserTest {
 public:
  AppListControllerBrowserTest()
    : profile2_(NULL) {}

  void OnProfileCreated(Profile* profile, Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      profile2_ = profile;
      base::MessageLoop::current()->Quit();
    }
  }

 protected:
  base::ScopedTempDir temp_profile_dir_;
  Profile* profile2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerBrowserTest);
};

// Test the CreateNewWindow function of the controller delegate.
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, CreateNewWindow) {
  const chrome::HostDesktopType desktop = chrome::GetActiveDesktop();
  AppListService* service = AppListService::Get();
  scoped_ptr<AppListControllerDelegate> controller(
      service->CreateControllerDelegate());
  ASSERT_TRUE(controller);

  EXPECT_EQ(1U, chrome::GetBrowserCount(browser()->profile(), desktop));
  EXPECT_EQ(0U, chrome::GetBrowserCount(
      browser()->profile()->GetOffTheRecordProfile(), desktop));

  controller->CreateNewWindow(browser()->profile(), false);
  EXPECT_EQ(2U, chrome::GetBrowserCount(browser()->profile(), desktop));

  controller->CreateNewWindow(browser()->profile(), true);
  EXPECT_EQ(1U, chrome::GetBrowserCount(
      browser()->profile()->GetOffTheRecordProfile(), desktop));
}

#if !defined(OS_CHROMEOS)
// Show the app list, then dismiss it.
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, ShowAndDismiss) {
  AppListService* service = AppListService::Get();
  ASSERT_FALSE(service->IsAppListVisible());
  service->ShowForProfile(browser()->profile());
  ASSERT_TRUE(service->IsAppListVisible());
  service->DismissAppList();
  ASSERT_FALSE(service->IsAppListVisible());
}

IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, SwitchAppListProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  profile_manager->CreateProfileAsync(
      temp_profile_dir_.path(),
      base::Bind(&AppListControllerBrowserTest::OnProfileCreated,
                 this),
      string16(), string16(), std::string());
  content::RunMessageLoop();  // Will stop in OnProfileCreated().

  AppListService* service = AppListService::Get();
  ASSERT_FALSE(service->IsAppListVisible());
  service->ShowForProfile(browser()->profile());
  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(browser()->profile(), service->GetCurrentAppListProfile());
  service->ShowForProfile(profile2_);
  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(profile2_, service->GetCurrentAppListProfile());
  service->DismissAppList();
  ASSERT_FALSE(service->IsAppListVisible());
}

class ShowAppListBrowserTest : public InProcessBrowserTest {
 public:
  ShowAppListBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
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
#endif  // !defined(OS_CHROMEOS)

// Browser Test for AppListController that observes search result changes.
class AppListControllerSearchResultsBrowserTest
    : public ExtensionBrowserTest,
      public app_list::SearchResultObserver,
      public ui::ListModelObserver {
 public:
  AppListControllerSearchResultsBrowserTest()
      : observed_result_(NULL),
        item_uninstall_count_(0),
        observed_results_list_(NULL) {}

  void WatchResultsLookingForItem(
      app_list::AppListModel::SearchResults* search_results,
      const std::string& extension_name) {
    EXPECT_FALSE(observed_results_list_);
    observed_results_list_ = search_results;
    observed_results_list_->AddObserver(this);
    item_to_observe_ = ASCIIToUTF16(extension_name);
  }

  void StopWatchingResults() {
    EXPECT_TRUE(observed_results_list_);
    observed_results_list_->RemoveObserver(this);
  }

 protected:
  void AttemptToLocateItem() {
    if (observed_result_) {
      observed_result_->RemoveObserver(this);
      observed_result_ = NULL;
    }

    for (size_t i = 0; i < observed_results_list_->item_count(); ++i) {
      if (observed_results_list_->GetItemAt(i)->title() != item_to_observe_)
        continue;

      // Ensure there is at most one.
      EXPECT_FALSE(observed_result_);
      observed_result_ = observed_results_list_->GetItemAt(i);
    }

    if (observed_result_)
      observed_result_->AddObserver(this);
  }

  // Overridden from SearchResultObserver:
  virtual void OnIconChanged() OVERRIDE {}
  virtual void OnActionsChanged() OVERRIDE {}
  virtual void OnIsInstallingChanged() OVERRIDE {}
  virtual void OnPercentDownloadedChanged() OVERRIDE {}
  virtual void OnItemInstalled() OVERRIDE {}
  virtual void OnItemUninstalled() OVERRIDE {
    ++item_uninstall_count_;
    EXPECT_TRUE(observed_result_);
  }

  // Overridden from ui::ListModelObserver:
  virtual void ListItemsAdded(size_t start, size_t count) OVERRIDE {
    AttemptToLocateItem();
  }
  virtual void ListItemsRemoved(size_t start, size_t count) OVERRIDE {
    AttemptToLocateItem();
  }
  virtual void ListItemMoved(size_t index, size_t target_index) OVERRIDE {}
  virtual void ListItemsChanged(size_t start, size_t count) OVERRIDE {}

  app_list::SearchResult* observed_result_;
  int item_uninstall_count_;

 private:
  base::string16 item_to_observe_;
  app_list::AppListModel::SearchResults* observed_results_list_;

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
  scoped_ptr<test::AppListServiceTestApi> test_api(
      test::AppListServiceTestApi::Create(chrome::HOST_DESKTOP_TYPE_NATIVE));
  ASSERT_TRUE(service);
  ASSERT_TRUE(test_api);
  service->ShowForProfile(browser()->profile());

  app_list::AppListModel* model = test_api->GetAppListModel();
  ASSERT_TRUE(model);
  model->SetSignedIn(true);
  WatchResultsLookingForItem(model->results(), extension->name());

  // Ensure a search finds the extension.
  EXPECT_FALSE(observed_result_);
  model->search_box()->SetText(ASCIIToUTF16("minimal"));
  EXPECT_TRUE(observed_result_);

  // Ensure the UI is updated. This is via PostTask in views.
  base::RunLoop().RunUntilIdle();

  // Now uninstall and ensure this browser test observes it.
  EXPECT_EQ(0, item_uninstall_count_);
  UninstallExtension(extension->id());
  EXPECT_EQ(1, item_uninstall_count_);

  // Results should not be immediately refreshed. When they are, the item should
  // be removed from the model.
  EXPECT_TRUE(observed_result_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observed_result_);
  StopWatchingResults();
  service->DismissAppList();
}

}  // namespace
