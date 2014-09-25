// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/browser_window_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserWindowLayoutTest : public testing::Test {
 public:
  BrowserWindowLayoutTest() {}
  virtual void SetUp() OVERRIDE {
    layout.reset([[BrowserWindowLayout alloc] init]);

    [layout setContentViewSize:NSMakeSize(600, 600)];
    [layout setWindowSize:NSMakeSize(600, 622)];
    [layout setInAnyFullscreen:NO];
    [layout setHasTabStrip:YES];
    [layout setHasToolbar:YES];
    [layout setToolbarHeight:32];
    [layout setPlaceBookmarkBarBelowInfoBar:NO];
    [layout setBookmarkBarHidden:NO];
    [layout setBookmarkBarHeight:26];
    [layout setInfoBarHeight:72];
    [layout setPageInfoBubblePointY:13];
    [layout setHasDownloadShelf:YES];
    [layout setDownloadShelfHeight:44];
  }

  base::scoped_nsobject<BrowserWindowLayout> layout;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserWindowLayoutTest);
};

TEST_F(BrowserWindowLayoutTest, TestAllViews) {
  chrome::LayoutOutput output = [layout computeLayout];

  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 585, 600, 37), output.tabStripFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 553, 600, 32), output.toolbarFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 527, 600, 26), output.bookmarkFrame));
  EXPECT_TRUE(NSEqualRects(NSZeroRect, output.fullscreenBackingBarFrame));
  EXPECT_EQ(527, output.findBarMaxY);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 455, 600, 111), output.infoBarFrame));
  EXPECT_TRUE(
      NSEqualRects(NSMakeRect(0, 0, 600, 44), output.downloadShelfFrame));
  EXPECT_TRUE(
      NSEqualRects(NSMakeRect(0, 44, 600, 411), output.contentAreaFrame));
}

TEST_F(BrowserWindowLayoutTest, TestAllViewsFullscreen) {
  // Content view has same size as window in AppKit Fullscreen.
  [layout setContentViewSize:NSMakeSize(600, 622)];
  [layout setInAnyFullscreen:YES];
  [layout setFullscreenSlidingStyle:fullscreen_mac::OMNIBOX_TABS_PRESENT];
  [layout setFullscreenMenubarOffset:0];
  [layout setFullscreenToolbarFraction:0];

  chrome::LayoutOutput output = [layout computeLayout];

  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 585, 600, 37), output.tabStripFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 553, 600, 32), output.toolbarFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 527, 600, 26), output.bookmarkFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 527, 600, 95),
                           output.fullscreenBackingBarFrame));
  EXPECT_EQ(527, output.findBarMaxY);
  EXPECT_EQ(527, output.fullscreenExitButtonMaxY);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 455, 600, 111), output.infoBarFrame));
  EXPECT_TRUE(
      NSEqualRects(NSMakeRect(0, 0, 600, 44), output.downloadShelfFrame));
  EXPECT_TRUE(
      NSEqualRects(NSMakeRect(0, 44, 600, 411), output.contentAreaFrame));
}

TEST_F(BrowserWindowLayoutTest, TestAllViewsFullscreenMenuBarShowing) {
  // Content view has same size as window in AppKit Fullscreen.
  [layout setContentViewSize:NSMakeSize(600, 622)];
  [layout setInAnyFullscreen:YES];
  [layout setFullscreenSlidingStyle:fullscreen_mac::OMNIBOX_TABS_PRESENT];
  [layout setFullscreenMenubarOffset:-10];
  [layout setFullscreenToolbarFraction:0];

  chrome::LayoutOutput output = [layout computeLayout];

  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 575, 600, 37), output.tabStripFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 543, 600, 32), output.toolbarFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 517, 600, 26), output.bookmarkFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 517, 600, 95),
                           output.fullscreenBackingBarFrame));
  EXPECT_EQ(517, output.findBarMaxY);
  EXPECT_EQ(517, output.fullscreenExitButtonMaxY);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 445, 600, 111), output.infoBarFrame));
  EXPECT_TRUE(
      NSEqualRects(NSMakeRect(0, 0, 600, 44), output.downloadShelfFrame));
  EXPECT_TRUE(
      NSEqualRects(NSMakeRect(0, 44, 600, 411), output.contentAreaFrame));
}

TEST_F(BrowserWindowLayoutTest, TestPopupWindow) {
  [layout setHasTabStrip:NO];
  [layout setHasToolbar:NO];
  [layout setHasLocationBar:YES];
  [layout setBookmarkBarHidden:YES];
  [layout setHasDownloadShelf:NO];

  chrome::LayoutOutput output = [layout computeLayout];

  EXPECT_TRUE(NSEqualRects(NSZeroRect, output.tabStripFrame));
  EXPECT_TRUE(NSEqualRects(NSMakeRect(1, 568, 598, 32), output.toolbarFrame));
  EXPECT_TRUE(NSEqualRects(NSZeroRect, output.bookmarkFrame));
  EXPECT_TRUE(NSEqualRects(NSZeroRect, output.fullscreenBackingBarFrame));
  EXPECT_EQ(567, output.findBarMaxY);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(0, 495, 600, 86), output.infoBarFrame));
  EXPECT_TRUE(NSEqualRects(NSZeroRect, output.downloadShelfFrame));
  EXPECT_TRUE(
      NSEqualRects(NSMakeRect(0, 0, 600, 495), output.contentAreaFrame));
}
