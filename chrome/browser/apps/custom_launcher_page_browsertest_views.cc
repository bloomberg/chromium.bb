// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_views.h"
#include "chrome/browser/ui/app_list/app_list_shower_views.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/views/controls/webview/webview.h"

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

  app_list::AppListView* GetAppListView() {
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
    return app_list_view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomLauncherPageBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest,
                       OpenLauncherAndSwitchToCustomPage) {
  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
  app_list::AppListView* app_list_view = GetAppListView();
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

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest, LauncherPageSubpages) {
  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");

  app_list::AppListView* app_list_view = GetAppListView();
  app_list::AppListModel* model = app_list_view->app_list_main_view()->model();
  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();

  ASSERT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  {
    ExtensionTestMessageListener listener("onPageProgressAt1", false);
    contents_view->SetActivePage(contents_view->GetPageIndexForState(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
    listener.WaitUntilSatisfied();
    EXPECT_TRUE(contents_view->IsStateActive(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
    // The app pushes 2 subpages when the launcher page is shown.
    EXPECT_EQ(2, model->custom_launcher_page_subpage_depth());
  }

  // Pop the subpages.
  {
    ExtensionTestMessageListener listener("onPopSubpage", false);
    EXPECT_TRUE(contents_view->Back());
    listener.WaitUntilSatisfied();
    EXPECT_TRUE(contents_view->IsStateActive(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
    EXPECT_EQ(1, model->custom_launcher_page_subpage_depth());
  }
  {
    ExtensionTestMessageListener listener("onPopSubpage", false);
    EXPECT_TRUE(contents_view->Back());
    listener.WaitUntilSatisfied();
    EXPECT_TRUE(contents_view->IsStateActive(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
    EXPECT_EQ(0, model->custom_launcher_page_subpage_depth());
  }

  // Once all subpages are popped, the start page should show.
  EXPECT_TRUE(contents_view->Back());

  // Immediately finish the animation.
  contents_view->Layout();
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));
}

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest,
                       LauncherPageShow) {
  const base::string16 kLauncherPageShowScript =
      base::ASCIIToUTF16("chrome.launcherPage.show();");

  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
  app_list::AppListView* app_list_view = GetAppListView();
  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();

  views::WebView* custom_page_view =
      static_cast<views::WebView*>(contents_view->custom_page_view());

  content::RenderFrameHost* custom_page_frame =
      custom_page_view->GetWebContents()->GetMainFrame();

  ASSERT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  // Ensure launcherPage.show() will switch the page to the custom launcher page
  // if the app launcher is already showing.
  {
    ExtensionTestMessageListener listener("onPageProgressAt1", false);
    custom_page_frame->ExecuteJavaScript(kLauncherPageShowScript);

    listener.WaitUntilSatisfied();
    EXPECT_TRUE(contents_view->IsStateActive(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
  }

  // Ensure launcherPage.show() will show the app list if it's hidden.
  {
    // Close the app list immediately.
    app_list_view->Close();
    app_list_view->GetWidget()->Close();

    ExtensionTestMessageListener listener("onPageProgressAt1", false);
    custom_page_frame->ExecuteJavaScript(kLauncherPageShowScript);

    listener.WaitUntilSatisfied();

    // The app list view will have changed on ChromeOS.
    app_list_view = GetAppListView();
    contents_view = app_list_view->app_list_main_view()->contents_view();
    EXPECT_TRUE(contents_view->IsStateActive(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
  }
}

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest, LauncherPageSetEnabled) {
  const base::string16 kLauncherPageDisableScript =
      base::ASCIIToUTF16("disableCustomLauncherPage();");
  const base::string16 kLauncherPageEnableScript =
      base::ASCIIToUTF16("enableCustomLauncherPage();");

  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
  app_list::AppListView* app_list_view = GetAppListView();
  app_list::AppListModel* model = app_list_view->app_list_main_view()->model();
  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();

  views::WebView* custom_page_view =
      static_cast<views::WebView*>(contents_view->custom_page_view());

  content::RenderFrameHost* custom_page_frame =
      custom_page_view->GetWebContents()->GetMainFrame();
  views::Widget* custom_page_click_zone =
      app_list_view->app_list_main_view()->GetCustomPageClickzone();

  ASSERT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  EXPECT_TRUE(custom_page_click_zone->GetLayer()->visible());
  EXPECT_TRUE(model->custom_launcher_page_enabled());
  {
    ExtensionTestMessageListener listener("launcherPageDisabled", false);
    custom_page_frame->ExecuteJavaScript(kLauncherPageDisableScript);

    listener.WaitUntilSatisfied();
    EXPECT_FALSE(custom_page_click_zone->GetLayer()->visible());
    EXPECT_FALSE(model->custom_launcher_page_enabled());
  }

  {
    ExtensionTestMessageListener listener("launcherPageEnabled", false);
    custom_page_frame->ExecuteJavaScript(kLauncherPageEnableScript);

    listener.WaitUntilSatisfied();
    EXPECT_TRUE(custom_page_click_zone->GetLayer()->visible());
    EXPECT_TRUE(model->custom_launcher_page_enabled());
  }
}
