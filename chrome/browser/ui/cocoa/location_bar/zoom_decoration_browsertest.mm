// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/test_toolbar_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/test_utils.h"

class ZoomDecorationTest : public InProcessBrowserTest {
 protected:
  ZoomDecorationTest()
      : InProcessBrowserTest(),
        should_quit_on_zoom_(false) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    zoom_subscription_ = content::HostZoomMap::GetDefaultForBrowserContext(
        browser()->profile())->AddZoomLevelChangedCallback(
            base::Bind(&ZoomDecorationTest::OnZoomChanged,
                       base::Unretained(this)));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    zoom_subscription_.reset();
  }

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
    chrome_page_zoom::Zoom(web_contents, zoom);
    content::RunMessageLoop();
  }

  void OnZoomChanged(const content::HostZoomMap::ZoomLevelChange& host) {
    if (should_quit_on_zoom_) {
      base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
          &base::MessageLoop::Quit,
          base::Unretained(base::MessageLoop::current())));
    }
  }

 private:
  bool should_quit_on_zoom_;
  scoped_ptr<content::HostZoomMap::Subscription> zoom_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ZoomDecorationTest);
};

IN_PROC_BROWSER_TEST_F(ZoomDecorationTest, BubbleAtDefaultZoom) {
  ZoomDecoration* zoom_decoration = GetZoomDecoration();

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
  EXPECT_TRUE(zoom_decoration->IsVisible());

  // Hide bubble and verify the decoration is hidden.
  zoom_decoration->CloseBubble();
  EXPECT_FALSE(zoom_decoration->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ZoomDecorationTest, HideOnInputProgress) {
  ZoomDecoration* zoom_decoration = GetZoomDecoration();

  // Zoom in and reset.
  Zoom(content::PAGE_ZOOM_IN);
  EXPECT_TRUE(zoom_decoration->IsVisible());

  scoped_ptr<ToolbarModel> toolbar_model(new TestToolbarModel);
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
