// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"

#include "chrome/browser/instant/instant_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/tab_contents/instant_preview_controller_mac.h"
#include "chrome/browser/ui/cocoa/tab_contents/preview_drop_shadow_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#import "testing/gtest_mac.h"

class PreviewableContentsControllerTest : public InProcessBrowserTest {
 public:
  PreviewableContentsControllerTest() : instant_model_(NULL) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile())));
    instant_model_.SetPreviewContents(web_contents_.get());

    controller_.reset([[PreviewableContentsController alloc]
         initWithBrowser:browser()
        windowController:nil]);
    [[controller_ view] setFrame:NSMakeRect(0, 0, 100, 200)];
    instant_model_.AddObserver([controller_ instantPreviewController]);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    instant_model_.RemoveObserver([controller_ instantPreviewController]);
    instant_model_.SetPreviewContents(NULL);
    controller_.reset();
    web_contents_.reset();
  }

  void VerifyPreviewFrame(CGFloat expected_height,
                          InstantSizeUnits units) {
    NSRect container_bounds = [[controller_ view] bounds];
    NSRect preview_frame =
        [web_contents_->GetView()->GetNativeView() frame];

    EXPECT_EQ(NSMinX(container_bounds), NSMinX(preview_frame));
    EXPECT_EQ(NSWidth(container_bounds), NSWidth(preview_frame));
    switch (units) {
      case INSTANT_SIZE_PIXELS:
        EXPECT_EQ(expected_height, NSHeight(preview_frame));
        EXPECT_EQ(NSMaxY(container_bounds), NSMaxY(preview_frame));
        break;
      case INSTANT_SIZE_PERCENT:
        EXPECT_EQ((expected_height * NSHeight(container_bounds)) / 100,
                   NSHeight(preview_frame));
        EXPECT_EQ(NSMaxY(container_bounds), NSMaxY(preview_frame));
    }
  }

 protected:
  InstantModel instant_model_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_nsobject<PreviewableContentsController> controller_;
};

// Verify that the view is correctly laid out when size is specified in percent.
IN_PROC_BROWSER_TEST_F(PreviewableContentsControllerTest, SizePerecent) {
  chrome::search::Mode mode;
  mode.mode = chrome::search::Mode::MODE_NTP;
  CGFloat expected_height = 30;
  InstantSizeUnits units = INSTANT_SIZE_PERCENT;
  instant_model_.SetPreviewState(mode, expected_height, units);

  EXPECT_NSEQ([web_contents_->GetView()->GetNativeView() superview],
              [controller_ view]);
  VerifyPreviewFrame(expected_height, units);

  // Resize the view and verify that the preview is also resized.
  [[controller_ view] setFrameSize:NSMakeSize(300, 400)];
  VerifyPreviewFrame(expected_height, units);
}

// Verify that the view is correctly laid out when size is specified in pixels.
IN_PROC_BROWSER_TEST_F(PreviewableContentsControllerTest, SizePixels) {
  chrome::search::Mode mode;
  mode.mode = chrome::search::Mode::MODE_NTP;
  CGFloat expected_height = 30;
  InstantSizeUnits units = INSTANT_SIZE_PIXELS;
  instant_model_.SetPreviewState(mode, expected_height, units);

  EXPECT_NSEQ([web_contents_->GetView()->GetNativeView() superview],
              [controller_ view]);
  VerifyPreviewFrame(expected_height, units);

  // Resize the view and verify that the preview is also resized.
  [[controller_ view] setFrameSize:NSMakeSize(300, 400)];
  VerifyPreviewFrame(expected_height, units);
}

// Verify that a shadow is not shown when the preview covers the entire page
// or when the preview is in NTP mode.
IN_PROC_BROWSER_TEST_F(PreviewableContentsControllerTest, NoShadowFullHeight) {
  chrome::search::Mode mode;
  mode.mode = chrome::search::Mode::MODE_SEARCH_SUGGESTIONS;
  instant_model_.SetPreviewState(mode, 100, INSTANT_SIZE_PERCENT);
  EXPECT_FALSE([controller_ dropShadowView]);
  EXPECT_FALSE([controller_ drawDropShadow]);

  mode.mode = chrome::search::Mode::MODE_NTP;
  instant_model_.SetPreviewState(mode, 10, INSTANT_SIZE_PERCENT);
  EXPECT_FALSE([controller_ dropShadowView]);
  EXPECT_FALSE([controller_ drawDropShadow]);
}

// Verify that a shadow is shown when the preview is in search mode.
IN_PROC_BROWSER_TEST_F(PreviewableContentsControllerTest, NoShadowNTP) {
  chrome::search::Mode mode;
  mode.mode = chrome::search::Mode::MODE_SEARCH_SUGGESTIONS;
  instant_model_.SetPreviewState(mode, 10, INSTANT_SIZE_PERCENT);
  EXPECT_TRUE([controller_ dropShadowView]);
  EXPECT_TRUE([controller_ drawDropShadow]);
  EXPECT_NSEQ([controller_ view], [[controller_ dropShadowView] superview]);

  NSRect dropShadowFrame = [[controller_ dropShadowView] frame];
  NSRect controllerBounds = [[controller_ view] bounds];
  EXPECT_EQ(NSWidth(controllerBounds), NSWidth(dropShadowFrame));
  EXPECT_EQ([PreviewDropShadowView preferredHeight],
            NSHeight(dropShadowFrame));
}

// Verify that the shadow is hidden when hiding the preview.
IN_PROC_BROWSER_TEST_F(PreviewableContentsControllerTest, HideShadow) {
  chrome::search::Mode mode;
  mode.mode = chrome::search::Mode::MODE_SEARCH_SUGGESTIONS;
  instant_model_.SetPreviewState(mode, 10, INSTANT_SIZE_PERCENT);
  EXPECT_TRUE([controller_ dropShadowView]);

  [controller_ onActivateTabWithContents:web_contents_.get()];
  EXPECT_FALSE([controller_ dropShadowView]);
}
