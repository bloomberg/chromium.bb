// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/app_list/app_list_service_cocoa_mac.h"

#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/test/base/in_process_browser_test.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"

using apps::AppShimHandler;

namespace test {

class AppListServiceMacTestApi {
 public:
  static AppListWindowController* window_controller() {
    return AppListServiceCocoaMac::GetInstance()->window_controller_;
  }
};

}  // namespace test

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

  AppListViewController* GetViewController() {
    return [test::AppListServiceMacTestApi::window_controller()
        appListViewController];
  }

  // testing::Test overrides:
  void TearDown() override {
    // At tear-down, NOTIFICATION_APP_TERMINATING should have been sent for the
    // browser shutdown. References to browser-owned objects must be removed
    // from the app list UI.
    AppListViewController* view_controller = GetViewController();
    // Note this first check will fail if the test doesn't ever show the
    // app list, but currently all tests in this file do.
    EXPECT_TRUE(view_controller);
    EXPECT_FALSE([view_controller delegate]);
  }

  // AppShimHandler::Host overrides:
  void OnAppLaunchComplete(apps::AppShimLaunchResult result) override {
    // AppList shims are always given APP_SHIM_LAUNCH_DUPLICATE_HOST, indicating
    // that the shim process should immediately close.
    EXPECT_EQ(apps::APP_SHIM_LAUNCH_DUPLICATE_HOST, result);
    ++launch_count_;
  }
  void OnAppClosed() override { NOTREACHED(); }
  void OnAppHide() override {}
  void OnAppUnhideWithoutActivation() override {}
  void OnAppRequestUserAttention(apps::AppShimAttentionType type) override {}
  base::FilePath GetProfilePath() const override {
    NOTREACHED();  // Currently unused in this test.
    return base::FilePath();
  }
  std::string GetAppId() const override { return app_mode::kAppListModeId; }

  int launch_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceMacInteractiveTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AppListServiceMacInteractiveTest, ShowAppListUsingShim) {
  // Check that AppListService has registered as a shim handler for "app_list".
  EXPECT_TRUE(AppShimHandler::GetForAppMode(app_mode::kAppListModeId));

  AppListService* service = AppListService::Get();
  EXPECT_FALSE(service->IsAppListVisible());

  // Creation should be lazy.
  EXPECT_FALSE(GetViewController());

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

  // View sticks around until shutdown.
  AppListViewController* view_controller = GetViewController();
  EXPECT_TRUE(view_controller);
  EXPECT_TRUE([view_controller delegate]);
}
