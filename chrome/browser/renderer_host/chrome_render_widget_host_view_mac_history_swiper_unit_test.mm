// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

#include "base/mac/scoped_nsobject.h"

#import "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"

@interface HistorySwiper (MacHistorySwiperTest)
- (BOOL)systemSettingsAllowHistorySwiping:(NSEvent*)event;
- (BOOL)browserCanNavigateInDirection:
        (history_swiper::NavigationDirection)forward
                                event:(NSEvent*)event;
- (void)endHistorySwipe;
- (void)beginHistorySwipeInDirection:
        (history_swiper::NavigationDirection)goForward
                               event:(NSEvent*)event;
- (void)navigateBrowserInDirection:(history_swiper::NavigationDirection)forward;
@end

class MacHistorySwiperTest : public CocoaTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();

    view_ = [[NSView alloc] init];
    id mockDelegate =
        [OCMockObject mockForProtocol:@protocol(HistorySwiperDelegate)];
    [[[mockDelegate stub] andReturn:view_] viewThatWantsHistoryOverlay];
    [[[mockDelegate stub] andReturnBool:YES] shouldAllowHistorySwiping];

    base::scoped_nsobject<HistorySwiper> historySwiper(
        [[HistorySwiper alloc] initWithDelegate:mockDelegate]);
    id mockHistorySwiper = [OCMockObject partialMockForObject:historySwiper];
    [[[mockHistorySwiper stub] andReturnBool:YES]
        systemSettingsAllowHistorySwiping:[OCMArg any]];
    [[[mockHistorySwiper stub] andReturnBool:YES]
        browserCanNavigateInDirection:history_swiper::kForwards
                                event:[OCMArg any]];
    [[[mockHistorySwiper stub] andReturnBool:YES]
        browserCanNavigateInDirection:history_swiper::kBackwards
                                event:[OCMArg any]];
    [[[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
      ++begin_count_;
      // beginHistorySwipeInDirection: calls endHistorySwipe internally.
      --end_count_;
    }] andForwardToRealObject]
        beginHistorySwipeInDirection:history_swiper::kForwards
                               event:[OCMArg any]];
    [[[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
      ++begin_count_;
      // beginHistorySwipeInDirection: calls endHistorySwipe internally.
      --end_count_;
    }] andForwardToRealObject]
        beginHistorySwipeInDirection:history_swiper::kBackwards
                               event:[OCMArg any]];
    [[[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
      ++end_count_;
    }] andForwardToRealObject] endHistorySwipe];
    [[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        navigated_right_ = true;
    }] navigateBrowserInDirection:history_swiper::kForwards];
    [[[mockHistorySwiper stub] andDo:^(NSInvocation* invocation) {
        navigated_left_ = true;
    }] navigateBrowserInDirection:history_swiper::kBackwards];
    historySwiper_ = [mockHistorySwiper retain];

    begin_count_ = 0;
    end_count_ = 0;
    navigated_right_ = false;
    navigated_left_ = false;
  }

  virtual void TearDown() OVERRIDE {
    [view_ release];
    [historySwiper_ release];
    CocoaTest::TearDown();
  }

  void startGestureInMiddle();
  void moveGestureInMiddle();
  void moveGestureAtPoint(NSPoint point);
  void momentumMoveGestureAtPoint(NSPoint point);
  void endGestureAtPoint(NSPoint point);

  HistorySwiper* historySwiper_;
  NSView* view_;
  int begin_count_;
  int end_count_;
  bool navigated_right_;
  bool navigated_left_;
};

NSPoint makePoint(CGFloat x, CGFloat y) {
  NSPoint point;
  point.x = x;
  point.y = y;
  return point;
}

id mockEventWithPoint(NSPoint point, NSEventType type) {
  id mockEvent = [OCMockObject mockForClass:[NSEvent class]];
  id mockTouch = [OCMockObject mockForClass:[NSTouch class]];
  [[[mockTouch stub] andReturnNSPoint:point] normalizedPosition];
  NSArray* touches = @[mockTouch];
  [[[mockEvent stub] andReturn:touches] touchesMatchingPhase:NSTouchPhaseAny
    inView:[OCMArg any]];
  [[[mockEvent stub] andReturnBool:NO] isDirectionInvertedFromDevice];
  [(NSEvent*)[[mockEvent stub] andReturnValue:OCMOCK_VALUE(type)] type];

  return mockEvent;
}

id scrollWheelEventWithPhase(NSEventPhase phase, NSEventPhase momentumPhase) {
  // The point isn't used, so we pass in bogus data.
  id event = mockEventWithPoint(makePoint(0,0), NSScrollWheel);
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(phase)] phase];
  [(NSEvent*)
      [[event stub] andReturnValue:OCMOCK_VALUE(momentumPhase)] momentumPhase];
  return event;
}

id scrollWheelEventWithPhase(NSEventPhase phase) {
  return scrollWheelEventWithPhase(phase, NSEventPhaseNone);
}

void MacHistorySwiperTest::startGestureInMiddle() {
  NSEvent* event = mockEventWithPoint(makePoint(0.5, 0.5), NSEventTypeGesture);
  [historySwiper_ touchesBeganWithEvent:event];
  [historySwiper_ beginGestureWithEvent:event];
  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseBegan);
  [historySwiper_ handleEvent:scrollEvent];
}

void MacHistorySwiperTest::moveGestureInMiddle() {
  moveGestureAtPoint(makePoint(0.5, 0.5));

  // Callbacks from blink to set the relevant state for history swiping.
  [historySwiper_ gotUnhandledWheelEvent];
  [historySwiper_ scrollOffsetPinnedToLeft:YES toRight:YES];
  [historySwiper_ setHasHorizontalScrollbar:NO];
}

void MacHistorySwiperTest::moveGestureAtPoint(NSPoint point) {
  NSEvent* event = mockEventWithPoint(point, NSEventTypeGesture);
  [historySwiper_ touchesMovedWithEvent:event];

  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseChanged);
  [historySwiper_ handleEvent:scrollEvent];
}

void MacHistorySwiperTest::momentumMoveGestureAtPoint(NSPoint point) {
  NSEvent* event = mockEventWithPoint(point, NSEventTypeGesture);
  [historySwiper_ touchesMovedWithEvent:event];

  NSEvent* scrollEvent =
      scrollWheelEventWithPhase(NSEventPhaseNone, NSEventPhaseChanged);
  [historySwiper_ handleEvent:scrollEvent];
}

void MacHistorySwiperTest::endGestureAtPoint(NSPoint point) {
  NSEvent* event = mockEventWithPoint(point, NSEventTypeGesture);
  [historySwiper_ touchesEndedWithEvent:event];

  NSEvent* scrollEvent = scrollWheelEventWithPhase(NSEventPhaseEnded);
  [historySwiper_ handleEvent:scrollEvent];
}

// Test that a simple left-swipe causes navigation.
TEST_F(MacHistorySwiperTest, SwipeLeft) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  startGestureInMiddle();
  moveGestureInMiddle();

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  moveGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_TRUE(navigated_left_);
}

// Test that a simple right-swipe causes navigation.
TEST_F(MacHistorySwiperTest, SwipeRight) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  startGestureInMiddle();
  moveGestureInMiddle();

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  moveGestureAtPoint(makePoint(0.8, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.8, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_TRUE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// If the user doesn't swipe enough, the history swiper should begin, but the
// browser should not navigate.
TEST_F(MacHistorySwiperTest, SwipeLeftSmallAmount) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  startGestureInMiddle();
  moveGestureInMiddle();
  moveGestureAtPoint(makePoint(0.45, 0.5));
  endGestureAtPoint(makePoint(0.45, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Diagonal swipes with a slight horizontal bias should not start the history
// swiper.
TEST_F(MacHistorySwiperTest, SwipeDiagonal) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  startGestureInMiddle();
  moveGestureInMiddle();
  moveGestureAtPoint(makePoint(0.6, 0.59));
  endGestureAtPoint(makePoint(0.6, 0.59));

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Swiping left and then down should cancel the history swiper without
// navigating.
TEST_F(MacHistorySwiperTest, SwipeLeftThenDown) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  startGestureInMiddle();
  moveGestureInMiddle();
  moveGestureAtPoint(makePoint(0.4, 0.5));
  moveGestureAtPoint(makePoint(0.4, 0.3));
  endGestureAtPoint(makePoint(0.2, 0.2));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);
}

// Sometimes Cocoa gets confused and sends us a momentum swipe event instead of
// a swipe gesture event. We should still function appropriately.
TEST_F(MacHistorySwiperTest, MomentumSwipeLeft) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  startGestureInMiddle();

  // Send a momentum move gesture.
  momentumMoveGestureAtPoint(makePoint(0.5, 0.5));
  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);

  // Callbacks from blink to set the relevant state for history swiping.
  [historySwiper_ gotUnhandledWheelEvent];
  [historySwiper_ scrollOffsetPinnedToLeft:YES toRight:YES];
  [historySwiper_ setHasHorizontalScrollbar:NO];

  momentumMoveGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 0);
  EXPECT_FALSE(navigated_right_);
  EXPECT_FALSE(navigated_left_);

  endGestureAtPoint(makePoint(0.2, 0.5));
  EXPECT_EQ(begin_count_, 1);
  EXPECT_EQ(end_count_, 1);
  EXPECT_FALSE(navigated_right_);
  EXPECT_TRUE(navigated_left_);
}

// User starts a swipe but doesn't move.
TEST_F(MacHistorySwiperTest, NoSwipe) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  startGestureInMiddle();
  moveGestureInMiddle();
  moveGestureAtPoint(makePoint(0.5, 0.5));
  endGestureAtPoint(makePoint(0.5, 0.5));

  EXPECT_EQ(begin_count_, 0);
  EXPECT_EQ(end_count_, 0);
}

// After a gesture is successfully recognized, momentum events should be
// swallowed, but new events should pass through.
TEST_F(MacHistorySwiperTest, TouchEventAfterGestureFinishes) {
  // These tests require 10.7+ APIs.
  if (![NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return;

  // Successfully pass through a gesture.
  startGestureInMiddle();
  moveGestureInMiddle();
  moveGestureAtPoint(makePoint(0.8, 0.5));
  endGestureAtPoint(makePoint(0.8, 0.5));
  EXPECT_TRUE(navigated_right_);

  // Momentum events should be swallowed.
  NSEvent* momentumEvent = scrollWheelEventWithPhase(NSEventPhaseNone);
  EXPECT_TRUE([historySwiper_ handleEvent:momentumEvent]);

  // New events should not be swallowed.
  NSEvent* beganEvent = scrollWheelEventWithPhase(NSEventPhaseBegan);
  EXPECT_FALSE([historySwiper_ handleEvent:beganEvent]);
}
