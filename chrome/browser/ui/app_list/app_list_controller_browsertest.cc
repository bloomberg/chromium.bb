// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
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

app_list::AppListModel* GetAppListModel(AppListService* service) {
  return app_list::AppListSyncableServiceFactory::GetForProfile(
      service->GetCurrentAppListProfile())->model();
}

AppListService* GetAppListService() {
  // TODO(tapted): Consider testing ash explicitly on the win-ash trybot.
  return AppListService::Get(chrome::GetActiveDesktop());
}

void SigninProfile(Profile* profile) {
  SigninManagerFactory::GetForProfile(profile)->
      SetAuthenticatedUsername("user@example.com");
}

}  // namespace

// Browser Test for AppListController that runs on all platforms supporting
// app_list.
class AppListControllerBrowserTest : public InProcessBrowserTest {
 public:
  AppListControllerBrowserTest()
    : profile2_(NULL) {}

  void InitSecondProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath temp_profile_dir =
        profile_manager->user_data_dir().AppendASCII("Profile 1");
    profile_manager->CreateProfileAsync(
        temp_profile_dir,
        base::Bind(&AppListControllerBrowserTest::OnProfileCreated,
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
  DISALLOW_COPY_AND_ASSIGN(AppListControllerBrowserTest);
};

// TODO(mgiuca): Enable on Linux when supported.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_CreateNewWindow DISABLED_CreateNewWindow
#else
#define MAYBE_CreateNewWindow CreateNewWindow
#endif

// Test the CreateNewWindow function of the controller delegate.
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, MAYBE_CreateNewWindow) {
  const chrome::HostDesktopType desktop = chrome::GetActiveDesktop();
  AppListService* service = GetAppListService();
  AppListControllerDelegate* controller(service->GetControllerDelegate());
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

// TODO(mgiuca): Enable on Linux/ChromeOS when supported.
#if defined(OS_LINUX)
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
IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest, MAYBE_ShowAndDismiss) {
  AppListService* service = GetAppListService();
  ASSERT_FALSE(service->IsAppListVisible());
  service->ShowForProfile(browser()->profile());
  ASSERT_TRUE(service->IsAppListVisible());
  service->DismissAppList();
  ASSERT_FALSE(service->IsAppListVisible());
}

IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest,
                       MAYBE_SwitchAppListProfiles) {
  InitSecondProfile();
  SigninProfile(browser()->profile());
  SigninProfile(profile2_);

  AppListService* service = GetAppListService();
  ASSERT_TRUE(service);

  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);

  // Open the app list with the browser's profile.
  ASSERT_FALSE(service->IsAppListVisible());
  controller->ShowForProfileByPath(browser()->profile()->GetPath());
  app_list::AppListModel* model = GetAppListModel(service);
  ASSERT_TRUE(model);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(browser()->profile(), service->GetCurrentAppListProfile());

  // Open the app list with the second profile.
  controller->ShowForProfileByPath(profile2_->GetPath());
  model = GetAppListModel(service);
  ASSERT_TRUE(model);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service->IsAppListVisible());
  ASSERT_EQ(profile2_, service->GetCurrentAppListProfile());

  controller->DismissView();
}

IN_PROC_BROWSER_TEST_F(AppListControllerBrowserTest,
                       MAYBE_SwitchAppListProfilesDuringSearch) {
  InitSecondProfile();
  SigninProfile(browser()->profile());
  SigninProfile(profile2_);

  AppListService* service = GetAppListService();
  ASSERT_TRUE(service);

  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);

  // Set a search with original profile.
  controller->ShowForProfileByPath(browser()->profile()->GetPath());
  app_list::AppListModel* model = GetAppListModel(service);
  ASSERT_TRUE(model);

  model->search_box()->SetText(ASCIIToUTF16("minimal"));
  base::RunLoop().RunUntilIdle();

  // Switch to the second profile.
  controller->ShowForProfileByPath(profile2_->GetPath());
  model = GetAppListModel(service);
  ASSERT_TRUE(model);
  base::RunLoop().RunUntilIdle();

  // Ensure the search box is empty.
  ASSERT_TRUE(model->search_box()->text().empty());
  ASSERT_EQ(profile2_, service->GetCurrentAppListProfile());

  controller->DismissView();
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

// See http://crbug.com/315677
#if defined(OS_WIN)
#define MAYBE_ShowAppListFlag DISABLED_ShowAppListFlag
#else
#define MAYBE_ShowAppListFlag ShowAppListFlag
#endif

IN_PROC_BROWSER_TEST_F(ShowAppListBrowserTest, MAYBE_ShowAppListFlag) {
  AppListService* service = GetAppListService();
  // The app list should already be shown because we passed
  // switches::kShowAppList.
  ASSERT_TRUE(service->IsAppListVisible());

  // Create a browser to prevent shutdown when we dismiss the app list.  We
  // need to do this because switches::kShowAppList suppresses the creation of
  // any browsers.
  CreateBrowser(service->GetCurrentAppListProfile());
  service->DismissAppList();
}

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

// TODO(mgiuca): Enable on Linux when supported.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_UninstallSearchResult DISABLED_UninstallSearchResult
#else
#define MAYBE_UninstallSearchResult UninstallSearchResult
#endif

// Test showing search results, and uninstalling one of them while displayed.
IN_PROC_BROWSER_TEST_F(AppListControllerSearchResultsBrowserTest,
                       UninstallSearchResult) {
  SigninProfile(browser()->profile());
  base::FilePath test_extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
  test_extension_path = test_extension_path.AppendASCII("extensions")
      .AppendASCII("platform_apps")
      .AppendASCII("minimal");
  const extensions::Extension* extension =
      InstallExtension(test_extension_path,
                       1 /* expected_change: new install */);
  ASSERT_TRUE(extension);

  AppListService* service = GetAppListService();
  ASSERT_TRUE(service);
  service->ShowForProfile(browser()->profile());

  app_list::AppListModel* model = GetAppListModel(service);
  ASSERT_TRUE(model);
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
