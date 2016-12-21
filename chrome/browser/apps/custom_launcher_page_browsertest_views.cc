// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/ash/app_list/test/app_list_service_ash_test_api.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/custom_launcher_page_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"

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
    AppListService* service = AppListService::Get();
    DCHECK(service);
    service->ShowForProfile(browser()->profile());
  }

  app_list::AppListView* GetAppListView() {
    app_list::AppListView* app_list_view = nullptr;
    AppListServiceAshTestApi service_test;
    app_list_view = service_test.GetAppListView();
    EXPECT_TRUE(service_test.GetAppListPresenter()->GetTargetVisibility());
    return app_list_view;
  }

  // Set the active page on the app list, according to |state|. Does not wait
  // for any animation or custom page to complete.
  void SetActiveStateAndVerify(app_list::AppListModel::State state) {
    app_list::ContentsView* contents_view =
        GetAppListView()->app_list_main_view()->contents_view();
    contents_view->SetActiveState(state);
    EXPECT_TRUE(contents_view->IsStateActive(state));
  }

  void SetCustomLauncherPageEnabled(bool enabled) {
    const base::string16 kLauncherPageDisableScript =
        base::ASCIIToUTF16("disableCustomLauncherPage();");
    const base::string16 kLauncherPageEnableScript =
        base::ASCIIToUTF16("enableCustomLauncherPage();");

    app_list::ContentsView* contents_view =
        GetAppListView()->app_list_main_view()->contents_view();
    views::WebView* custom_page_view = static_cast<views::WebView*>(
        contents_view->custom_page_view()->custom_launcher_page_contents());
    content::RenderFrameHost* custom_page_frame =
        custom_page_view->GetWebContents()->GetMainFrame();

    const char* test_message =
        enabled ? "launcherPageEnabled" : "launcherPageDisabled";

    ExtensionTestMessageListener listener(test_message, false);
    custom_page_frame->ExecuteJavaScriptForTests(
        enabled ? kLauncherPageEnableScript : kLauncherPageDisableScript);
    listener.WaitUntilSatisfied();
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
    contents_view->SetActiveState(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);

    listener.WaitUntilSatisfied();
  }
  {
    ExtensionTestMessageListener listener("onPageProgressAt0", false);
    contents_view->SetActiveState(app_list::AppListModel::STATE_START);

    listener.WaitUntilSatisfied();
  }
}

// Test that the app list will switch to the custom launcher page by sending a
// click inside the clickzone, or a mouse scroll event.
IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest,
                       EventsActivateSwitchToCustomPage) {
  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
  // Use an event generator to ensure targeting is correct.
  app_list::AppListView* app_list_view = GetAppListView();

  // On ChromeOS, displaying the app list can be delayed while icons finish
  // loading. Explicitly show it to ensure the event generator gets meaningful
  // coordinates. See http://crbug.com/525128.
  app_list_view->GetWidget()->Show();

  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();
  gfx::NativeWindow window = app_list_view->GetWidget()->GetNativeWindow();
  ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  // Find the clickzone.
  gfx::Rect bounds =
      contents_view->custom_page_view()->GetCollapsedLauncherPageBounds();
  bounds.Intersect(contents_view->bounds());
  gfx::Point point_in_clickzone = bounds.CenterPoint();
  views::View::ConvertPointToWidget(contents_view, &point_in_clickzone);

  // First try clicking 10px above the clickzone.
  gfx::Point point_above_clickzone = point_in_clickzone;
  point_above_clickzone.set_y(bounds.y() - 10);
  views::View::ConvertPointToWidget(contents_view, &point_above_clickzone);

  event_generator.MoveMouseRelativeTo(window, point_above_clickzone);
  event_generator.ClickLeftButton();

  // Should stay on the start page.
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  // Now click in the clickzone.
  event_generator.MoveMouseRelativeTo(window, point_in_clickzone);
  // First, try disabling the custom page view. Click should do nothing.
  SetCustomLauncherPageEnabled(false);
  event_generator.ClickLeftButton();
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));
  // Click again with it enabled. The active state should update immediately.
  SetCustomLauncherPageEnabled(true);
  event_generator.ClickLeftButton();
  EXPECT_TRUE(contents_view->IsStateActive(
      app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));

  // Back to the start page. And send a mouse wheel event.
  SetActiveStateAndVerify(app_list::AppListModel::STATE_START);
  // Generate wheel events above the clickzone.
  event_generator.MoveMouseRelativeTo(window, point_above_clickzone);
  // Scrolling left, right or up should do nothing.
  event_generator.MoveMouseWheel(-5, 0);
  event_generator.MoveMouseWheel(5, 0);
  event_generator.MoveMouseWheel(0, 5);
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));
  // Scroll down to open launcher page.
  event_generator.MoveMouseWheel(0, -5);
  EXPECT_TRUE(contents_view->IsStateActive(
      app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));

  // Constants for gesture/trackpad events.
  const base::TimeDelta step_delay = base::TimeDelta::FromMilliseconds(300);
  const int num_steps = 5;
  const int num_fingers = 2;

  // Gesture events need to be in host coordinates. The points need to be put
  // into screen coordinates. This works because the root window assumes it
  // fills the screen.
  point_in_clickzone = bounds.CenterPoint();
  point_above_clickzone.SetPoint(point_in_clickzone.x(), bounds.y() - 10);
  views::View::ConvertPointToScreen(contents_view, &point_above_clickzone);
  views::View::ConvertPointToScreen(contents_view, &point_in_clickzone);

  // Back to the start page. And send a scroll gesture.
  SetActiveStateAndVerify(app_list::AppListModel::STATE_START);
  // Going down should do nothing.
  event_generator.GestureScrollSequence(
      point_above_clickzone, point_in_clickzone, step_delay, num_steps);
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));
  // Now go up - should change state.
  event_generator.GestureScrollSequence(
      point_in_clickzone, point_above_clickzone, step_delay, num_steps);
  EXPECT_TRUE(contents_view->IsStateActive(
      app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));

  // Back to the start page. And send a trackpad scroll event.
  SetActiveStateAndVerify(app_list::AppListModel::STATE_START);
  // Going down left, right or up should do nothing.
  event_generator.ScrollSequence(point_in_clickzone, step_delay, -5, 0,
                                 num_steps, num_fingers);
  event_generator.ScrollSequence(point_in_clickzone, step_delay, 5, 0,
                                 num_steps, num_fingers);
  event_generator.ScrollSequence(point_in_clickzone, step_delay, 0, 5,
                                 num_steps, num_fingers);
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));
  // Scroll up to open launcher page.
  event_generator.ScrollSequence(point_in_clickzone, step_delay, 0, -5,
                                 num_steps, num_fingers);
  EXPECT_TRUE(contents_view->IsStateActive(
      app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));

  // Back to the start page. And send a tap gesture.
  SetActiveStateAndVerify(app_list::AppListModel::STATE_START);
  // Tapping outside the clickzone should do nothing.
  event_generator.GestureTapAt(point_above_clickzone);
  EXPECT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));
  // Now tap in the clickzone.
  event_generator.GestureTapAt(point_in_clickzone);
  EXPECT_TRUE(contents_view->IsStateActive(
      app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
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
    contents_view->SetActiveState(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
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

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest, LauncherPageShowAndHide) {
  const base::string16 kLauncherPageShowScript =
      base::ASCIIToUTF16("chrome.launcherPage.show();");
  const base::string16 kLauncherPageHideScript =
      base::ASCIIToUTF16("hideCustomLauncherPage()");

  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
  app_list::AppListView* app_list_view = GetAppListView();
  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();

  views::WebView* custom_page_view = static_cast<views::WebView*>(
      contents_view->custom_page_view()->custom_launcher_page_contents());

  content::RenderFrameHost* custom_page_frame =
      custom_page_view->GetWebContents()->GetMainFrame();

  ASSERT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  // Ensure launcherPage.show() will switch the page to the custom launcher page
  // if the app launcher is already showing.
  {
    ExtensionTestMessageListener listener("onPageProgressAt1", false);
    custom_page_frame->ExecuteJavaScriptForTests(kLauncherPageShowScript);

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
    custom_page_frame->ExecuteJavaScriptForTests(kLauncherPageShowScript);

    listener.WaitUntilSatisfied();

    // The app list view will have changed on ChromeOS.
    app_list_view = GetAppListView();
    contents_view = app_list_view->app_list_main_view()->contents_view();
    EXPECT_TRUE(contents_view->IsStateActive(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE));
  }

  // Ensure launcherPage.hide() hides the launcher page when it's showing.
  {
    ExtensionTestMessageListener listener("onPageProgressAt0", false);
    custom_page_frame->ExecuteJavaScriptForTests(kLauncherPageHideScript);

    listener.WaitUntilSatisfied();

    EXPECT_TRUE(
        contents_view->IsStateActive(app_list::AppListModel::STATE_START));
  }

  // Nothing should happen if hide() is called from the apps page.
  {
    contents_view->SetActiveState(app_list::AppListModel::STATE_APPS, false);

    ExtensionTestMessageListener listener("launcherPageHidden", false);
    custom_page_frame->ExecuteJavaScriptForTests(kLauncherPageHideScript);
    listener.WaitUntilSatisfied();

    EXPECT_TRUE(
        contents_view->IsStateActive(app_list::AppListModel::STATE_APPS));
  }
}

IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest, LauncherPageSetEnabled) {
  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
  app_list::AppListView* app_list_view = GetAppListView();
  app_list::AppListModel* model = app_list_view->app_list_main_view()->model();
  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();

  views::View* custom_page_view = contents_view->custom_page_view();
  ASSERT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  EXPECT_TRUE(model->custom_launcher_page_enabled());
  EXPECT_TRUE(custom_page_view->visible());

  SetCustomLauncherPageEnabled(false);
  EXPECT_FALSE(model->custom_launcher_page_enabled());
  EXPECT_FALSE(custom_page_view->visible());

  SetCustomLauncherPageEnabled(true);
  EXPECT_TRUE(model->custom_launcher_page_enabled());
  EXPECT_TRUE(custom_page_view->visible());
}

// Currently this is flaky.
// Disabled test http://crbug.com/463456
IN_PROC_BROWSER_TEST_F(CustomLauncherPageBrowserTest,
                       LauncherPageFocusTraversal) {
  LoadAndLaunchPlatformApp(kCustomLauncherPagePath, "Launched");
  app_list::AppListView* app_list_view = GetAppListView();
  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();
  app_list::SearchBoxView* search_box_view =
      app_list_view->app_list_main_view()->search_box_view();

  ASSERT_TRUE(
      contents_view->IsStateActive(app_list::AppListModel::STATE_START));

  {
    ExtensionTestMessageListener listener("onPageProgressAt1", false);
    contents_view->SetActiveState(
        app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
    listener.WaitUntilSatisfied();
  }

  // Expect that the search box and webview are the only two focusable views.
  views::View* search_box_textfield = search_box_view->search_box();
  views::WebView* custom_page_webview = static_cast<views::WebView*>(
      contents_view->custom_page_view()->custom_launcher_page_contents());
  EXPECT_EQ(custom_page_webview,
            app_list_view->GetFocusManager()->GetNextFocusableView(
                search_box_textfield, search_box_textfield->GetWidget(), false,
                false));
  EXPECT_EQ(
      search_box_textfield,
      app_list_view->GetFocusManager()->GetNextFocusableView(
          custom_page_webview, custom_page_webview->GetWidget(), false, false));

  // And in reverse.
  EXPECT_EQ(
      search_box_textfield,
      app_list_view->GetFocusManager()->GetNextFocusableView(
          custom_page_webview, custom_page_webview->GetWidget(), true, false));
  EXPECT_EQ(custom_page_webview,
            app_list_view->GetFocusManager()->GetNextFocusableView(
                search_box_textfield, search_box_textfield->GetWidget(), true,
                false));
}
