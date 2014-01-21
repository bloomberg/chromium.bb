// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_mac.h"

#include <vector>

#include "apps/app_shim/app_shim_handler_mac.h"
#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/test/base/in_process_browser_test.h"

using apps::AppShimHandler;

namespace {

// Browser test for mac-specific AppListService functionality.
class AppListServiceMacInteractiveTest : public InProcessBrowserTest,
                                         public AppShimHandler::Host {
 public:
  AppListServiceMacInteractiveTest() : launch_count_(0) {}

 protected:
  void LaunchShim() {
    // AppList shims always launch normally (never relaunched via dock icon).
    AppShimHandler::GetForAppMode(app_mode::kAppListModeId)->
        OnShimLaunch(this,
                     apps::APP_SHIM_LAUNCH_NORMAL,
                     std::vector<base::FilePath>());
  }

  // AppShimHandler::Host overrides:
  virtual void OnAppLaunchComplete(apps::AppShimLaunchResult result) OVERRIDE {
    // AppList shims are always given APP_SHIM_LAUNCH_DUPLICATE_HOST, indicating
    // that the shim process should immediately close.
    EXPECT_EQ(apps::APP_SHIM_LAUNCH_DUPLICATE_HOST, result);
    ++launch_count_;
  }
  virtual void OnAppClosed() OVERRIDE {
    NOTREACHED();
  }
  virtual void OnAppHide() OVERRIDE {}
  virtual void OnAppRequestUserAttention() OVERRIDE {}
  virtual base::FilePath GetProfilePath() const OVERRIDE {
    NOTREACHED();  // Currently unused in this test.
    return base::FilePath();
  }
  virtual std::string GetAppId() const OVERRIDE {
    return app_mode::kAppListModeId;
  }

  int launch_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceMacInteractiveTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AppListServiceMacInteractiveTest,
                       ShowAppListUsingShim) {
  // Check that AppListService has registered as a shim handler for "app_list".
  EXPECT_TRUE(AppShimHandler::GetForAppMode(app_mode::kAppListModeId));

  AppListService* service =
      AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE);
  EXPECT_FALSE(service->IsAppListVisible());

  // With no saved profile, the default profile should be chosen and saved.
  service->Show();
  EXPECT_EQ(browser()->profile(), service->GetCurrentAppListProfile());
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(0, launch_count_);
  service->DismissAppList();
  EXPECT_FALSE(service->IsAppListVisible());

  // Test showing the app list via the shim. The shim should immediately close
  // leaving the app list visible.
  LaunchShim();
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(1, launch_count_);

  // Launching again should toggle the app list.
  LaunchShim();
  EXPECT_FALSE(service->IsAppListVisible());
  EXPECT_EQ(2, launch_count_);

  // Test showing the app list via the shim, then dismissing the app list.
  LaunchShim();
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(3, launch_count_);
  service->DismissAppList();
  EXPECT_FALSE(service->IsAppListVisible());
}
