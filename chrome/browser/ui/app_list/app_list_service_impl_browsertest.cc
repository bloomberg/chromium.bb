// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/constants.h"
#include "ui/base/models/menu_model.h"

namespace test {

// Test API to access private members of AppListServiceImpl.
class AppListServiceImplTestApi {
 public:
  explicit AppListServiceImplTestApi(AppListServiceImpl* impl) : impl_(impl) {}

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

// Tests that the AppListViewDelegate is created lazily.
IN_PROC_BROWSER_TEST_F(AppListServiceImplBrowserTest, CreatedLazily) {
  EXPECT_FALSE(test_api_->view_delegate());
  service_->GetViewDelegate();
  EXPECT_TRUE(test_api_->view_delegate());
}

// Test that all the items in the context menu for a hosted app have valid
// labels.
IN_PROC_BROWSER_TEST_F(AppListServiceImplBrowserTest, ShowContextMenu) {
  AppListService* service = AppListService::Get();
  EXPECT_TRUE(service);

  // Show the app list to ensure it has loaded a profile.
  service_->Show();
  AppListModelUpdater* model_updater = test::GetModelUpdater(service);
  EXPECT_TRUE(model_updater);

  // Get the webstore hosted app, which is always present.
  ChromeAppListItem* item = model_updater->FindItem(extensions::kWebStoreAppId);
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
