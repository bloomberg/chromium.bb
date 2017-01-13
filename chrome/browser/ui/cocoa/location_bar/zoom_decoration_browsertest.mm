// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/toolbar/test_toolbar_model.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/test_utils.h"

class ZoomDecorationTest : public InProcessBrowserTest {
 protected:
  ZoomDecorationTest()
      : InProcessBrowserTest(),
        should_quit_on_zoom_(false) {
  }

  void SetUpOnMainThread() override {
    zoom_subscription_ = content::HostZoomMap::GetDefaultForBrowserContext(
        browser()->profile())->AddZoomLevelChangedCallback(
            base::Bind(&ZoomDecorationTest::OnZoomChanged,
                       base::Unretained(this)));
  }

  void TearDownOnMainThread() override { zoom_subscription_.reset(); }

  LocationBarViewMac* GetLocationBar() const {
    BrowserWindowController* controller =
        [BrowserWindowController browserWindowControllerForWindow:
            browser()->window()->GetNativeWindow()];
    return [controller locationBarBridge];
  }

  ZoomDecoration* GetZoomDecoration() const {
    return GetLocationBar()->zoom_decoration_.get();
  }

  ZoomDecoration* GetZoomDecorationForBrowser(Browser* browser) const {
    BrowserWindowController* controller =
        [BrowserWindowController browserWindowControllerForWindow:
            browser->window()->GetNativeWindow()];
    return [controller locationBarBridge]->zoom_decoration_.get();
  }

  void Zoom(content::PageZoom zoom) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    base::AutoReset<bool> reset(&should_quit_on_zoom_, true);
    zoom::PageZoom::Zoom(web_contents, zoom);
    content::RunMessageLoop();
  }

  void OnZoomChanged(const content::HostZoomMap::ZoomLevelChange& host) {
    if (should_quit_on_zoom_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&base::MessageLoop::QuitWhenIdle,
                     base::Unretained(base::MessageLoop::current())));
    }
  }

 private:
  bool should_quit_on_zoom_;
  std::unique_ptr<content::HostZoomMap::Subscription> zoom_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ZoomDecorationTest);
};

IN_PROC_BROWSER_TEST_F(ZoomDecorationTest, BubbleAtDefaultZoom) {
  ZoomDecoration* zoom_decoration = GetZoomDecoration();

  // TODO(wjmaclean): This shouldn't be necessary, but at present this test
  // assumes the various Zoom() calls do not invoke a notification
  // bubble, which prior to https://codereview.chromium.org/940673002/
  // was accomplished by not showing the bubble for inactive windows.
  // Since we now need to be able to show the zoom bubble as a notification
  // on non-active pages, this test should be revised to account for
  // these notifications.
  zoom::ZoomController::FromWebContents(GetLocationBar()->GetWebContents())
      ->SetShowsNotificationBubble(false);

  // Zoom in and reset.
  EXPECT_FALSE(zoom_decoration->IsVisible());
  Zoom(content::PAGE_ZOOM_IN);
  EXPECT_TRUE(zoom_decoration->IsVisible());
  Zoom(content::PAGE_ZOOM_RESET);
  EXPECT_FALSE(zoom_decoration->IsVisible());

  // Zoom in and show bubble then reset.
  Zoom(content::PAGE_ZOOM_IN);
  EXPECT_TRUE(zoom_decoration->IsVisible());
  zoom_decoration->ShowBubble(false);
  Zoom(content::PAGE_ZOOM_RESET);
  EXPECT_FALSE(zoom_decoration->IsVisible());

  // Hide bubble and verify the decoration is hidden.
  zoom_decoration->CloseBubble();
  EXPECT_FALSE(zoom_decoration->IsVisible());
}

// Regression test for https://crbug.com/462482.
IN_PROC_BROWSER_TEST_F(ZoomDecorationTest, IconRemainsVisibleAfterBubble) {
  ZoomDecoration* zoom_decoration = GetZoomDecoration();

  // See comment in BubbleAtDefaultZoom regarding this next line.
  zoom::ZoomController::FromWebContents(GetLocationBar()->GetWebContents())
      ->SetShowsNotificationBubble(false);

  // Zoom in to turn on decoration icon.
  EXPECT_FALSE(zoom_decoration->IsVisible());
  Zoom(content::PAGE_ZOOM_IN);
  EXPECT_TRUE(zoom_decoration->IsVisible());

  // Show zoom bubble, verify decoration icon remains visible.
  zoom_decoration->ShowBubble(/* auto_close = */false);
  EXPECT_TRUE(zoom_decoration->IsVisible());

  // Close bubble and verify the decoration is still visible.
  zoom_decoration->CloseBubble();
  EXPECT_TRUE(zoom_decoration->IsVisible());

  // Verify the decoration does go away when we expect it to.
  Zoom(content::PAGE_ZOOM_RESET);
  EXPECT_FALSE(zoom_decoration->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ZoomDecorationTest, HideOnInputProgress) {
  ZoomDecoration* zoom_decoration = GetZoomDecoration();

  // Zoom in and reset.
  Zoom(content::PAGE_ZOOM_IN);
  EXPECT_TRUE(zoom_decoration->IsVisible());

  std::unique_ptr<ToolbarModel> toolbar_model(new TestToolbarModel);
  toolbar_model->set_input_in_progress(true);
  browser()->swap_toolbar_models(&toolbar_model);
  GetLocationBar()->ZoomChangedForActiveTab(false);
  EXPECT_FALSE(zoom_decoration->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ZoomDecorationTest, CloseBrowserWithOpenBubble) {
  chrome::SetZoomBubbleAutoCloseDelayForTesting(0);

  // Create a new browser so that it can be closed properly.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ZoomDecoration* zoom_decoration = GetZoomDecorationForBrowser(browser2);
  zoom_decoration->ShowBubble(true);

  // Test shouldn't crash.
  browser2->window()->Close();
  content::RunAllPendingInMessageLoop();
}
