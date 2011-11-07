// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"

namespace {
const CGFloat kBubbleWindowWidth = 100;
const CGFloat kBubbleWindowHeight = 50;
const CGFloat kAnchorPointX = 400;
const CGFloat kAnchorPointY = 300;
} // namespace

class BaseBubbleControllerTest : public CocoaTest {
 public:
  virtual void SetUp() OVERRIDE {
    bubbleWindow_.reset([[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, kBubbleWindowWidth,
                                       kBubbleWindowHeight)
                  styleMask:NSBorderlessWindowMask
                    backing:NSBackingStoreBuffered
                      defer:YES]);

    // The bubble controller will release itself when the window closes.
    controller_ = [[BaseBubbleController alloc]
        initWithWindow:bubbleWindow_.get()
          parentWindow:test_window()
            anchoredAt:NSMakePoint(kAnchorPointX, kAnchorPointY)];
    EXPECT_TRUE([controller_ bubble]);
  }

  virtual void TearDown() OVERRIDE {
    // Close our windows.
    [controller_ close];
    bubbleWindow_.reset(NULL);
    CocoaTest::TearDown();
  }

 public:
  scoped_nsobject<NSWindow> bubbleWindow_;
  BaseBubbleController* controller_;
};

// Test that kAlignEdgeToAnchorEdge and a left bubble arrow correctly aligns the
// left edge of the buble to the anchor point.
TEST_F(BaseBubbleControllerTest, LeftAlign) {
  [[controller_ bubble] setArrowLocation:info_bubble::kTopLeft];
  [[controller_ bubble] setAlignment:info_bubble::kAlignEdgeToAnchorEdge];
  [controller_ showWindow:nil];

  NSRect frame = [[controller_ window] frame];
  // Make sure the bubble size hasn't changed.
  EXPECT_EQ(frame.size.width, kBubbleWindowWidth);
  EXPECT_EQ(frame.size.height, kBubbleWindowHeight);
  // Make sure the bubble is left aligned.
  EXPECT_EQ(NSMinX(frame), kAnchorPointX);
  EXPECT_GE(NSMaxY(frame), kAnchorPointY);
}

// Test that kAlignEdgeToAnchorEdge and a right bubble arrow correctly aligns
// the right edge of the buble to the anchor point.
TEST_F(BaseBubbleControllerTest, RightAlign) {
  [[controller_ bubble] setArrowLocation:info_bubble::kTopRight];
  [[controller_ bubble] setAlignment:info_bubble::kAlignEdgeToAnchorEdge];
  [controller_ showWindow:nil];

  NSRect frame = [[controller_ window] frame];
  // Make sure the bubble size hasn't changed.
  EXPECT_EQ(frame.size.width, kBubbleWindowWidth);
  EXPECT_EQ(frame.size.height, kBubbleWindowHeight);
  // Make sure the bubble is left aligned.
  EXPECT_EQ(NSMaxX(frame), kAnchorPointX);
  EXPECT_GE(NSMaxY(frame), kAnchorPointY);
}

// Test that kAlignArrowToAnchor and a left bubble arrow correctly aligns
// the bubble arrow to the anchor point.
TEST_F(BaseBubbleControllerTest, AnchorAlignLeftArrow) {
  [[controller_ bubble] setArrowLocation:info_bubble::kTopLeft];
  [[controller_ bubble] setAlignment:info_bubble::kAlignArrowToAnchor];
  [controller_ showWindow:nil];

  NSRect frame = [[controller_ window] frame];
  // Make sure the bubble size hasn't changed.
  EXPECT_EQ(frame.size.width, kBubbleWindowWidth);
  EXPECT_EQ(frame.size.height, kBubbleWindowHeight);
  // Make sure the bubble arrow points to the anchor.
  EXPECT_EQ(NSMinX(frame) + info_bubble::kBubbleArrowXOffset +
      roundf(info_bubble::kBubbleArrowWidth / 2.0), kAnchorPointX);
  EXPECT_GE(NSMaxY(frame), kAnchorPointY);
}

// Test that kAlignArrowToAnchor and a right bubble arrow correctly aligns
// the bubble arrow to the anchor point.
TEST_F(BaseBubbleControllerTest, AnchorAlignRightArrow) {
  [[controller_ bubble] setArrowLocation:info_bubble::kTopRight];
  [[controller_ bubble] setAlignment:info_bubble::kAlignArrowToAnchor];
  [controller_ showWindow:nil];

  NSRect frame = [[controller_ window] frame];
  // Make sure the bubble size hasn't changed.
  EXPECT_EQ(frame.size.width, kBubbleWindowWidth);
  EXPECT_EQ(frame.size.height, kBubbleWindowHeight);
  // Make sure the bubble arrow points to the anchor.
  EXPECT_EQ(NSMaxX(frame) - info_bubble::kBubbleArrowXOffset -
      floorf(info_bubble::kBubbleArrowWidth / 2.0), kAnchorPointX);
  EXPECT_GE(NSMaxY(frame), kAnchorPointY);
}
