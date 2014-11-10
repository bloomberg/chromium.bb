// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_views.h"
#include "chrome/browser/ui/app_list/app_list_shower_views.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"

namespace {

// The path of the test application within the "platform_apps" directory.
const char kCustomLauncherPagePath[] = "custom_launcher_page";

// The app ID of the test application.
const char kCustomLauncherPageID[] = "lmadimbbgapmngbiclpjjngmdickadpl";

}  // namespace

// Browser tests for custom launcher pages, platform apps that run as a page in
// the app launcher. Within this test class, LoadAndLaunchPlatformApp runs the
// app inside the launcher, not as a standalone background page.
// the app launcher.
class CustomLauncherPageBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  CustomLauncherPageBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);

    // Custom launcher pages only work in the experimental app list.
    command_line->AppendSwitch(app_list::switches::kEnableExperimentalAppList);

    // Ensure the app list does not close during the test.
    command_line->AppendSwitch(
        app_list::switches::kDisableAppListDismissOnBlur);

    // The test app must be whitelisted to use launcher_page.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kCustomLauncherPageID);
  }

  // Open the launcher. Ignores the Extension argument (this will simply
  // activate any loaded launcher pages).
  void LaunchPlatformApp(const extensions::Extension* /*unused*/) override {
    AppListService* service =
        AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE);
    DCHECK(service);
    service->ShowForProfile(browser()->profile());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomLauncherPageBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest,
                       OpenLauncherAndSwitchToCustomPage) {
  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");

  app_list::AppListView* app_list_view = nullptr;
#if defined(OS_CHROMEOS)
  ash::Shell* shell = ash::Shell::GetInstance();
  app_list_view = shell->GetAppListView();
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
#else
  AppListServiceViews* service = static_cast<AppListServiceViews*>(
      AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE));
  // The app list should have loaded instantly since the profile is already
  // loaded.
  EXPECT_TRUE(service->IsAppListVisible());
  app_list_view = service->shower().app_list();
#endif

  ASSERT_NE(nullptr, app_list_view);
  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();

  ASSERT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  {
    ExtensionTestMessageListener listener("onPageProgressAt1", false);
    contents_view->SetActivePage(contents_view->GetPageIndexForState(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));

    listener.WaitUntilSatisfied();
  }
  {
    ExtensionTestMessageListener listener("onPageProgressAt0", false);
    contents_view->SetActivePage(contents_view->GetPageIndexForState(
        app_list::AppListModel::STATE_START));

    listener.WaitUntilSatisfied();
  }
}
