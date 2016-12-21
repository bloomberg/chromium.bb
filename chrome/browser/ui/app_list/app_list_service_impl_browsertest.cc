// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/base/models/menu_model.h"

namespace test {

// Test API to access private members of AppListServiceImpl.
class AppListServiceImplTestApi {
 public:
  explicit AppListServiceImplTestApi(AppListServiceImpl* impl) : impl_(impl) {}

  ProfileLoader* profile_loader() { return impl_->profile_loader_.get(); }
  AppListViewDelegate* view_delegate() { return impl_->view_delegate_.get(); }

 private:
  AppListServiceImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceImplTestApi);
};

}  // namespace test

// Browser Test for AppListServiceImpl that runs on all platforms supporting
// app_list.
class AppListServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  AppListServiceImplBrowserTest() {}

  // Overridden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    service_ = test::GetAppListServiceImpl();
    test_api_.reset(new test::AppListServiceImplTestApi(service_));
  }

 protected:
  AppListServiceImpl* service_;
  std::unique_ptr<test::AppListServiceImplTestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceImplBrowserTest);
};

// Test that showing a loaded profile for the first time is lazy and
// synchronous. Then tests that showing a second loaded profile without
// dismissing correctly switches profiles.
// crbug.com/459649
IN_PROC_BROWSER_TEST_F(AppListServiceImplBrowserTest,
                       DISABLED_ShowLoadedProfiles) {
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kAppListProfile));

  // When never shown, profile path should match the last used profile.
  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  EXPECT_EQ(service_->GetProfilePath(user_data_dir),
            browser()->profile()->GetPath());

  // Just requesting the profile path shouldn't set it.
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kAppListProfile));

  // The app list service is bound to ChromeLauncherController, which should
  // always have a profile.
  EXPECT_TRUE(service_->GetCurrentAppListProfile());

  // Showing the app list for an unspecified profile, uses the loaded profile.
  service_->Show();

  // Load should be synchronous.
  EXPECT_FALSE(test_api_->profile_loader()->IsAnyProfileLoading());
  EXPECT_EQ(service_->GetCurrentAppListProfile(), browser()->profile());

  // ChromeOS doesn't record the app list profile pref, and doesn't do profile
  // switching.
  EXPECT_FALSE(local_state->HasPrefPath(prefs::kAppListProfile));
}

// Tests that the AppListViewDelegate is created lazily.
IN_PROC_BROWSER_TEST_F(AppListServiceImplBrowserTest, CreatedLazily) {
  EXPECT_FALSE(test_api_->view_delegate());
  service_->ShowForProfile(browser()->profile());
  EXPECT_TRUE(test_api_->view_delegate());
}

// Test that all the items in the context menu for a hosted app have valid
// labels.
IN_PROC_BROWSER_TEST_F(AppListServiceImplBrowserTest, ShowContextMenu) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNewBookmarkApps);
  AppListService* service = AppListService::Get();
  EXPECT_TRUE(service);

  // Show the app list to ensure it has loaded a profile.
  service_->ShowForProfile(browser()->profile());
  app_list::AppListModel* model = test::GetAppListModel(service);
  EXPECT_TRUE(model);

  // Get the webstore hosted app, which is always present.
  app_list::AppListItem* item = model->FindItem(extensions::kWebStoreAppId);
  EXPECT_TRUE(item);

  ui::MenuModel* menu_model = item->GetContextMenuModel();
  EXPECT_TRUE(menu_model);

  int num_items = menu_model->GetItemCount();
  EXPECT_LT(0, num_items);

  for (int i = 0; i < num_items; i++) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR)
      continue;

    base::string16 label = menu_model->GetLabelAt(i);
    EXPECT_FALSE(label.empty());
  }
}
