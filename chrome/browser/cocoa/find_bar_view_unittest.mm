// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/find_bar_view.h"
#include "chrome/browser/cocoa/test_event_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface MouseDownViewPong : NSView {
  BOOL pong_;
}
@property(assign) BOOL pong;
@end

@implementation MouseDownViewPong
@synthesize pong = pong_;
- (void)mouseDown:(NSEvent*)event {
  pong_ = YES;
}
@end


namespace {

class FindBarViewTest : public PlatformTest {
 public:
  FindBarViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    view_.reset([[FindBarView alloc] initWithFrame:frame]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  scoped_nsobject<FindBarView> view_;
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(FindBarViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(FindBarViewTest, Display) {
  [view_ display];
}

TEST_F(FindBarViewTest, FindBarEatsMouseClicksInBackgroundArea) {
  MouseDownViewPong* pongView =
      [[MouseDownViewPong alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)];

  // Remove all of the subviews of the findbar, to make sure we don't
  // accidentally hit a subview when trying to simulate a click in the
  // background area.
  [view_ setSubviews:[NSArray array]];
  [view_ setFrame:NSMakeRect(0, 0, 200, 200)];

  // Add the pong view as a sibling of the findbar.
  [cocoa_helper_.contentView() addSubview:pongView
                               positioned:NSWindowBelow
                               relativeTo:view_.get()];

  // Synthesize a mousedown event and send it to the window.  The event is
  // placed in the center of the find bar.
  NSPoint pointInCenterOfFindBar = NSMakePoint(100, 100);
  [pongView setPong:NO];
  [cocoa_helper_.window()
      sendEvent:test_event_utils::LeftMouseDownAtPoint(pointInCenterOfFindBar)];
  // Click gets eaten by findbar, not passed through to underlying view.
  EXPECT_FALSE([pongView pong]);
}

TEST_F(FindBarViewTest, FindBarPassesThroughClicksInTransparentArea) {
  MouseDownViewPong* pongView =
      [[MouseDownViewPong alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)];
  [view_ setFrame:NSMakeRect(0, 0, 200, 200)];

  // Add the pong view as a sibling of the findbar.
  [cocoa_helper_.contentView() addSubview:pongView
                               positioned:NSWindowBelow
                               relativeTo:view_.get()];

  // Synthesize a mousedown event and send it to the window.  The event is inset
  // a few pixels from the lower left corner of the window, which places it in
  // the transparent area surrounding the findbar.
  NSPoint pointInTransparentArea = NSMakePoint(2, 2);
  [pongView setPong:NO];
  [cocoa_helper_.window()
      sendEvent:test_event_utils::LeftMouseDownAtPoint(pointInTransparentArea)];
  // Click is ignored by findbar, passed through to underlying view.
  EXPECT_TRUE([pongView pong]);
}
}  // namespace
