// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_handler_mac.h"
#include "base/command_line.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/test/base/in_process_browser_test.h"

using apps::AppShimHandler;

namespace {

// Browser test for mac-specific AppListService functionality.
class AppListServiceMacBrowserTest : public InProcessBrowserTest,
                                     public AppShimHandler::Host {
 public:
  AppListServiceMacBrowserTest() : launch_count_(0),
                                   close_count_(0),
                                   running_(false) {}

 protected:
  void LaunchShim() {
    DCHECK(!running_);
    // AppList shims should always succced showing the app list.
    AppShimHandler::GetForAppMode(app_mode::kAppListModeId)->
        OnShimLaunch(this, apps::APP_SHIM_LAUNCH_NORMAL);
    running_ = true;
  }

  void FocusShim() {
    DCHECK(running_);
    AppShimHandler::GetForAppMode(app_mode::kAppListModeId)->
        OnShimFocus(this, apps::APP_SHIM_FOCUS_REOPEN);
  }

  void QuitShim() {
    DCHECK(running_);
    running_ = false;
    AppShimHandler::GetForAppMode(app_mode::kAppListModeId)->OnShimClose(this);
  }

  // AppShimHandler::Host overrides:
  virtual void OnAppLaunchComplete(apps::AppShimLaunchResult) OVERRIDE {
    ++launch_count_;
  }
  virtual void OnAppClosed() OVERRIDE {
    ++close_count_;
    QuitShim();
  }
  virtual base::FilePath GetProfilePath() const OVERRIDE {
    NOTREACHED();  // Currently unused in this test.
    return base::FilePath();
  }
  virtual std::string GetAppId() const OVERRIDE {
    return app_mode::kAppListModeId;
  }

  int launch_count_;
  int close_count_;
  bool running_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceMacBrowserTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AppListServiceMacBrowserTest, ShowAppListUsingShim) {
  // Check that AppListService has registered as a shim handler for "app_list".
  EXPECT_TRUE(AppShimHandler::GetForAppMode(app_mode::kAppListModeId));

  AppListService* service = AppListService::Get();
  EXPECT_FALSE(service->IsAppListVisible());
  EXPECT_EQ(0, close_count_);

  // With no saved profile, the default profile should be chosen and saved.
  service->Show();
  EXPECT_EQ(browser()->profile(), service->GetCurrentAppListProfile());
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(0, launch_count_);
  EXPECT_EQ(0, close_count_);
  service->DismissAppList();
  EXPECT_FALSE(service->IsAppListVisible());

  // There is no shim yet, so the close count should not change.
  EXPECT_EQ(0, close_count_);

  // Test showing the app list via the shim, then quit the shim directly. The
  // close count does not change since the shim host is triggering the close,
  // not the app list.
  LaunchShim();
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(1, launch_count_);
  QuitShim();
  EXPECT_FALSE(service->IsAppListVisible());
  EXPECT_EQ(0, close_count_);

  // Test showing the app list via the shim, then simulate clicking the dock
  // icon again, which should close it.
  LaunchShim();
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(2, launch_count_);
  FocusShim();
  EXPECT_FALSE(service->IsAppListVisible());
  EXPECT_EQ(1, close_count_);

  // Test showing the app list via the shim, then dismissing the app list.
  LaunchShim();
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(3, launch_count_);
  service->DismissAppList();
  EXPECT_FALSE(service->IsAppListVisible());
  EXPECT_EQ(2, close_count_);

  // Verify that observers are correctly removed by ensuring that |close_count_|
  // is unchanged when the app list is dismissed again.
  service->ShowForProfile(browser()->profile());
  EXPECT_TRUE(service->IsAppListVisible());
  EXPECT_EQ(3, launch_count_);
  service->DismissAppList();
  EXPECT_FALSE(service->IsAppListVisible());
  EXPECT_EQ(2, close_count_);
}
