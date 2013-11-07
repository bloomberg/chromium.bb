// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/run_loop_testing.h"
#import "ui/base/test/cocoa_test_event_utils.h"

namespace {
const CGFloat kBubbleWindowWidth = 100;
const CGFloat kBubbleWindowHeight = 50;
const CGFloat kAnchorPointX = 400;
const CGFloat kAnchorPointY = 300;
}  // namespace

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
  base::scoped_nsobject<NSWindow> bubbleWindow_;
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

// Tests that when a new window gets key state (and the bubble resigns) that
// the key window changes.
TEST_F(BaseBubbleControllerTest, ResignKeyCloses) {
  // Closing the bubble will autorelease the controller.
  base::scoped_nsobject<BaseBubbleController> keep_alive([controller_ retain]);

  NSWindow* bubble_window = [controller_ window];
  EXPECT_FALSE([bubble_window isVisible]);

  base::scoped_nsobject<NSWindow> other_window(
      [[NSWindow alloc] initWithContentRect:NSMakeRect(500, 500, 500, 500)
                                  styleMask:NSTitledWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:YES]);
  EXPECT_FALSE([other_window isVisible]);

  [controller_ showWindow:nil];
  EXPECT_TRUE([bubble_window isVisible]);
  EXPECT_FALSE([other_window isVisible]);

  [other_window makeKeyAndOrderFront:nil];
  // Fake the key state notification. Because unit_tests is a "daemon" process
  // type, its windows can never become key (nor can the app become active).
  // Instead of the hacks below, one could make a browser_test or transform the
  // process type, but this seems easiest and is best suited to a unit test.
  //
  // On Lion and above, which have the event taps, simply post a notification
  // that will cause the controller to call |-windowDidResignKey:|. Earlier
  // OSes can call through directly.
  NSNotification* notif =
      [NSNotification notificationWithName:NSWindowDidResignKeyNotification
                                    object:bubble_window];
  if (base::mac::IsOSLionOrLater())
    [[NSNotificationCenter defaultCenter] postNotification:notif];
  else
    [controller_ windowDidResignKey:notif];


  EXPECT_FALSE([bubble_window isVisible]);
  EXPECT_TRUE([other_window isVisible]);
}

// Test that clicking outside the window causes the bubble to close if
// shouldCloseOnResignKey is YES.
TEST_F(BaseBubbleControllerTest, LionClickOutsideCloses) {
  // The event tap is only installed on 10.7+.
  if (!base::mac::IsOSLionOrLater())
    return;

  // Closing the bubble will autorelease the controller.
  base::scoped_nsobject<BaseBubbleController> keep_alive([controller_ retain]);
  NSWindow* window = [controller_ window];

  EXPECT_TRUE([controller_ shouldCloseOnResignKey]);  // Verify default value.
  EXPECT_FALSE([window isVisible]);

  [controller_ showWindow:nil];

  EXPECT_TRUE([window isVisible]);

  [controller_ setShouldCloseOnResignKey:NO];
  NSEvent* event = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
      NSMakePoint(10, 10), test_window());
  [NSApp sendEvent:event];
  chrome::testing::NSRunLoopRunAllPending();

  EXPECT_TRUE([window isVisible]);

  [controller_ setShouldCloseOnResignKey:YES];
  event = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
      NSMakePoint(10, 10), test_window());
  [NSApp sendEvent:event];
  chrome::testing::NSRunLoopRunAllPending();

  EXPECT_FALSE([window isVisible]);
}
