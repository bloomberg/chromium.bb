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
#include "chrome/browser/ui/cocoa/location_bar/mock_toolbar_model.h"
#import "chrome/browser/ui/cocoa/location_bar/zoom_decoration.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/test_utils.h"

class ZoomDecorationTest : public InProcessBrowserTest {
 protected:
  ZoomDecorationTest()
      : InProcessBrowserTest(),
        old_toolbar_model_(NULL),
        should_quit_on_zoom_(false),
        zoom_callback_(base::Bind(&ZoomDecorationTest::OnZoomChanged,
                                  base::Unretained(this))) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    content::HostZoomMap::GetForBrowserContext(
        browser()->profile())->AddZoomLevelChangedCallback(zoom_callback_);

    old_toolbar_model_ = GetLocationBar()->toolbar_model_;
    GetLocationBar()->toolbar_model_ = &mock_toolbar_model_;
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    content::HostZoomMap::GetForBrowserContext(
        browser()->profile())->RemoveZoomLevelChangedCallback(zoom_callback_);
    GetLocationBar()->toolbar_model_ = old_toolbar_model_;
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

  ZoomBubbleController* GetBubble() const {
    return GetZoomDecoration()->bubble_;
  }

  void Zoom(content::PageZoom zoom) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    base::AutoReset<bool> reset(&should_quit_on_zoom_, true);
    chrome_page_zoom::Zoom(web_contents, zoom);
    content::RunMessageLoop();
  }

  void OnZoomChanged(const std::string& host) {
    if (should_quit_on_zoom_) {
      MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
          &MessageLoop::Quit, base::Unretained(MessageLoop::current())));
    }
  }

  chrome::testing::MockToolbarModel mock_toolbar_model_;

 private:
  ToolbarModel* old_toolbar_model_;
  bool should_quit_on_zoom_;
  content::HostZoomMap::ZoomLevelChangedCallback zoom_callback_;

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
  zoom_decoration->ToggleBubble(false);
  Zoom(content::PAGE_ZOOM_RESET);
  EXPECT_TRUE(zoom_decoration->IsVisible());

  // Hide bubble and verify the decoration is hidden.
  [GetBubble() close];
  EXPECT_FALSE(zoom_decoration->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ZoomDecorationTest, HideOnInputProgress) {
  ZoomDecoration* zoom_decoration = GetZoomDecoration();

  // Zoom in and reset.
  Zoom(content::PAGE_ZOOM_IN);
  EXPECT_TRUE(zoom_decoration->IsVisible());

  mock_toolbar_model_.SetInputInProgress(true);
  GetLocationBar()->ZoomChangedForActiveTab(false);
  EXPECT_FALSE(zoom_decoration->IsVisible());
}
