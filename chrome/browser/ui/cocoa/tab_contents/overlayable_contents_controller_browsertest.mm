// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/tab_contents/instant_overlay_controller_mac.h"
#include "chrome/browser/ui/cocoa/tab_contents/overlay_separator_view.h"
#include "chrome/browser/ui/search/instant_overlay_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#import "testing/gtest_mac.h"

class OverlayableContentsControllerTest : public InProcessBrowserTest,
                                          public content::NotificationObserver {
 public:
  OverlayableContentsControllerTest() : instant_overlay_model_(NULL),
                                        visibility_changed_count_(0) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile())));
    instant_overlay_model_.SetOverlayContents(web_contents_.get());

    BrowserWindowController* window_controller =
        [BrowserWindowController browserWindowControllerForWindow:
                browser()->window()->GetNativeWindow()];
    controller_.reset([[OverlayableContentsController alloc]
         initWithBrowser:browser()
        windowController:window_controller]);
    [[controller_ view] setFrame:NSMakeRect(0, 0, 100, 200)];
    instant_overlay_model_.AddObserver([controller_ instantOverlayController]);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    instant_overlay_model_.RemoveObserver(
        [controller_ instantOverlayController]);
    instant_overlay_model_.SetOverlayContents(NULL);
    controller_.reset();
    web_contents_.reset();
  }

  void VerifyOverlayFrame(CGFloat expected_height,
                          InstantSizeUnits units) {
    NSRect container_bounds = [[controller_ view] bounds];
    container_bounds.size.height -= [OverlayTopSeparatorView preferredHeight];
    NSRect overlay_frame =
        [web_contents_->GetView()->GetNativeView() frame];

    EXPECT_EQ(NSMinX(container_bounds), NSMinX(overlay_frame));
    EXPECT_EQ(NSWidth(container_bounds), NSWidth(overlay_frame));
    switch (units) {
      case INSTANT_SIZE_PIXELS:
        EXPECT_EQ(expected_height, NSHeight(overlay_frame));
        EXPECT_EQ(NSMaxY(container_bounds), NSMaxY(overlay_frame));
        break;
      case INSTANT_SIZE_PERCENT:
        EXPECT_EQ((expected_height * NSHeight(container_bounds)) / 100,
                   NSHeight(overlay_frame));
        EXPECT_EQ(NSMaxY(container_bounds), NSMaxY(overlay_frame));
    }
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED)
      ++visibility_changed_count_;
  }

 protected:
  InstantOverlayModel instant_overlay_model_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_nsobject<OverlayableContentsController> controller_;
  content::NotificationRegistrar registrar_;
  int visibility_changed_count_;
};

// Verify that the view is correctly laid out when size is specified in percent.
IN_PROC_BROWSER_TEST_F(OverlayableContentsControllerTest, SizePerecent) {
  SearchMode mode;
  mode.mode = SearchMode::MODE_NTP;
  CGFloat expected_height = 30;
  InstantSizeUnits units = INSTANT_SIZE_PERCENT;
  instant_overlay_model_.SetOverlayState(mode, expected_height, units);

  EXPECT_NSEQ([web_contents_->GetView()->GetNativeView() superview],
              [controller_ view]);
  VerifyOverlayFrame(expected_height, units);

  // Resize the view and verify that the overlay is also resized.
  [[controller_ view] setFrameSize:NSMakeSize(300, 400)];
  VerifyOverlayFrame(expected_height, units);
}

// Verify that the view is correctly laid out when size is specified in pixels.
IN_PROC_BROWSER_TEST_F(OverlayableContentsControllerTest, SizePixels) {
  SearchMode mode;
  mode.mode = SearchMode::MODE_NTP;
  CGFloat expected_height = 30;
  InstantSizeUnits units = INSTANT_SIZE_PIXELS;
  instant_overlay_model_.SetOverlayState(mode, expected_height, units);

  EXPECT_NSEQ([web_contents_->GetView()->GetNativeView() superview],
              [controller_ view]);
  VerifyOverlayFrame(expected_height, units);

  // Resize the view and verify that the overlay is also resized.
  [[controller_ view] setFrameSize:NSMakeSize(300, 400)];
  VerifyOverlayFrame(expected_height, units);
}

// Verify that a bottom border is not shown when the overlay covers the entire
// page or when the overlay is in NTP mode.
IN_PROC_BROWSER_TEST_F(OverlayableContentsControllerTest, NoShadowFullHeight) {
  SearchMode mode;
  mode.mode = SearchMode::MODE_SEARCH_SUGGESTIONS;
  instant_overlay_model_.SetOverlayState(mode, 100, INSTANT_SIZE_PERCENT);
  EXPECT_FALSE([controller_ dropShadowView]);
  EXPECT_FALSE([controller_ drawDropShadow]);

  mode.mode = SearchMode::MODE_NTP;
  instant_overlay_model_.SetOverlayState(mode, 10, INSTANT_SIZE_PERCENT);
  EXPECT_FALSE([controller_ dropShadowView]);
  EXPECT_FALSE([controller_ drawDropShadow]);
}

// Verify that a shadow is shown when the overlay is in search mode.
IN_PROC_BROWSER_TEST_F(OverlayableContentsControllerTest, NoShadowNTP) {
  SearchMode mode;
  mode.mode = SearchMode::MODE_SEARCH_SUGGESTIONS;
  instant_overlay_model_.SetOverlayState(mode, 10, INSTANT_SIZE_PERCENT);
  EXPECT_TRUE([controller_ dropShadowView]);
  EXPECT_TRUE([controller_ drawDropShadow]);
  EXPECT_NSEQ([controller_ view], [[controller_ dropShadowView] superview]);

  NSRect dropShadowFrame = [[controller_ dropShadowView] frame];
  NSRect controllerBounds = [[controller_ view] bounds];
  EXPECT_EQ(NSWidth(controllerBounds), NSWidth(dropShadowFrame));
  EXPECT_EQ([OverlayBottomSeparatorView preferredHeight],
            NSHeight(dropShadowFrame));
}

// Verify that the shadow is hidden when hiding the overlay.
IN_PROC_BROWSER_TEST_F(OverlayableContentsControllerTest, HideShadow) {
  SearchMode mode;
  mode.mode = SearchMode::MODE_SEARCH_SUGGESTIONS;
  instant_overlay_model_.SetOverlayState(mode, 10, INSTANT_SIZE_PERCENT);
  EXPECT_TRUE([controller_ dropShadowView]);

  [controller_ onActivateTabWithContents:web_contents_.get()];
  EXPECT_FALSE([controller_ dropShadowView]);
}

// Verify that the web contents is not hidden when just the height changes.
IN_PROC_BROWSER_TEST_F(OverlayableContentsControllerTest, HeightChangeNoHide) {
  SearchMode mode;
  mode.mode = SearchMode::MODE_SEARCH_SUGGESTIONS;
  instant_overlay_model_.SetOverlayState(mode, 10, INSTANT_SIZE_PERCENT);

  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
                 content::Source<content::WebContents>(web_contents_.get()));
  EXPECT_EQ(0, visibility_changed_count_);
  instant_overlay_model_.SetOverlayState(mode, 11, INSTANT_SIZE_PERCENT);
  EXPECT_EQ(1, visibility_changed_count_);
}
