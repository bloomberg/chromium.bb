// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/time.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"

namespace ash {

typedef test::AshTestBase SystemGestureEventFilterTest;

// Ensure that events targeted at the root window are consumed by the
// system event handler.
TEST_F(SystemGestureEventFilterTest, TapOutsideRootWindow) {
  aura::RootWindow* root_window = Shell::GetRootWindow();

  Shell::TestApi shell_test(Shell::GetInstance());

  const int kTouchId = 5;

  // A touch outside the root window will be associated with the root window
  aura::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(-10, -10), kTouchId,
      base::Time::NowFromSystemTime() - base::Time());
  root_window->DispatchTouchEvent(&press);

  aura::GestureEvent* event = new aura::GestureEvent(
      ui::ET_GESTURE_TAP, 0, 0, 0, base::Time::Now(), 0, 0, 1 << kTouchId);
  bool consumed = root_window->DispatchGestureEvent(event);

  EXPECT_TRUE(consumed);

  // Without the event filter, the touch shouldn't be consumed by the
  // system event handler.
  Shell::GetInstance()->RemoveRootWindowEventFilter(
      shell_test.system_gesture_event_filter());

  aura::GestureEvent* event2 = new aura::GestureEvent(
      ui::ET_GESTURE_TAP, 0, 0, 0, base::Time::Now(), 0, 0, 1 << kTouchId);
  consumed = root_window->DispatchGestureEvent(event2);

  // The event filter doesn't exist, so the touch won't be consumed.
  EXPECT_FALSE(consumed);
}

// Ensure that a three fingered swipe is consumed by the system event handler.
TEST_F(SystemGestureEventFilterTest, ThreeFingerSwipe) {
  aura::RootWindow* root_window = Shell::GetRootWindow();

  const int kTouchId = 5;

  // Get a target for kTouchId
  aura::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(100, 100), kTouchId,
                         base::Time::NowFromSystemTime() - base::Time());
  root_window->DispatchTouchEvent(&press);

  aura::GestureEvent* event = new aura::GestureEvent(
      ui::ET_GESTURE_THREE_FINGER_SWIPE, 0, 0, 0, base::Time::Now(),
      0, 0, 1 << kTouchId);
  bool consumed = root_window->DispatchGestureEvent(event);

  EXPECT_TRUE(consumed);

  // The system event filter shouldn't filter out events like tap downs.
  aura::GestureEvent* event2 = new aura::GestureEvent(
      ui::ET_GESTURE_TAP_DOWN, 0, 0, 0, base::Time::Now(),
      0, 0, 1 << kTouchId);
  consumed = root_window->DispatchGestureEvent(event2);

  EXPECT_FALSE(consumed);
}

}  // namespace ash
