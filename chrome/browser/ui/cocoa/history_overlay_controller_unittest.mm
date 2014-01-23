// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_overlay_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_mac.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class HistoryOverlayControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();

    // The overlay controller shows the panel as a subview of the given view.
    test_view_.reset([[NSView alloc] initWithFrame:NSMakeRect(10, 10, 10, 10)]);

    // We add it to the test_window for authenticity.
    [[test_window() contentView] addSubview:test_view_];
  }

  NSView* test_view() {
    return test_view_;
  }

 private:
  base::scoped_nsobject<NSView> test_view_;
};

// Tests that when the controller is |-dismiss|ed, the animation runs and then
// is removed when the animation completes. The view should be added and
// removed at the appropriate times.
TEST_F(HistoryOverlayControllerTest, DismissClearsAnimationsAndRemovesView) {
  EXPECT_EQ(0u, [[test_view() subviews] count]);

  base::scoped_nsobject<HistoryOverlayController> controller(
      [[HistoryOverlayController alloc] initForMode:kHistoryOverlayModeBack]);
  [controller showPanelForView:test_view()];
  EXPECT_EQ(1u, [[test_view() subviews] count]);

  scoped_ptr<base::MessagePumpNSRunLoop> message_pump(
      new base::MessagePumpNSRunLoop);

  id mock = [OCMockObject partialMockForObject:controller];
  [mock setExpectationOrderMatters:YES];
  [[[mock expect] andForwardToRealObject] dismiss];

  // Called after |-animationDidStop:finished:|.
  base::MessagePumpNSRunLoop* weak_message_pump = message_pump.get();
  void (^quit_loop)(NSInvocation* invocation) = ^(NSInvocation* invocation) {
      weak_message_pump->Quit();
  };
  // Set up the mock to first forward to the real implementation and then call
  // the above block to quit the run loop.
  [[[[mock expect] andForwardToRealObject] andDo:quit_loop]
      animationDidStop:[OCMArg isNotNil] finished:YES];

  // CAAnimations must be committed within a run loop. It is not sufficient
  // to commit them and activate the loop after the fact. Schedule a block to
  // dismiss the controller from within the run loop, which begins the
  // animation.
  CFRunLoopPerformBlock(CFRunLoopGetCurrent(),
                        kCFRunLoopDefaultMode,
                        ^(void) {
      [mock dismiss];
  });

  // Run the loop, which will dismiss the overlay.
  message_pump->Run(NULL);

  EXPECT_OCMOCK_VERIFY(mock);

  // After the animation runs, there should be no more animations.
  EXPECT_FALSE([[controller view] animations]);

  EXPECT_EQ(0u, [[test_view() subviews] count]);
}
