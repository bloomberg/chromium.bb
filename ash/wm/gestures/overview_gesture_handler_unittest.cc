// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"

namespace ash {

class OverviewGestureHandlerTest : public test::AshTestBase {
 public:
  OverviewGestureHandlerTest() {}
  virtual ~OverviewGestureHandlerTest() {}

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithDelegate(&delegate_, -1, bounds);
  }

  bool IsSelecting() {
    return ash::Shell::GetInstance()->window_selector_controller()->
        IsSelecting();
  }

 private:
  aura::test::TestWindowDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverviewGestureHandlerTest);
};

// Tests a swipe up with three fingers to enter and a swipe down to exit
// overview.
TEST_F(OverviewGestureHandlerTest, VerticalSwipes) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::EventGenerator generator(root_window, root_window);
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
      0, -500, 100, 3);
  EXPECT_TRUE(IsSelecting());

  // Swiping up again does nothing.
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
      0, -500, 100, 3);
  EXPECT_TRUE(IsSelecting());

  // Swiping down exits.
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
      0, 500, 100, 3);
  EXPECT_FALSE(IsSelecting());

  // Swiping down again does nothing.
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
      0, 500, 100, 3);
  EXPECT_FALSE(IsSelecting());
}

// Tests that a mostly horizontal swipe does not trigger overview.
TEST_F(OverviewGestureHandlerTest, HorizontalSwipes) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::EventGenerator generator(root_window, root_window);
  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
      600, -500, 100, 3);
  EXPECT_FALSE(IsSelecting());

  generator.ScrollSequence(gfx::Point(), base::TimeDelta::FromMilliseconds(5),
      -600, -500, 100, 3);
  EXPECT_FALSE(IsSelecting());
}

// Tests a swipe up with three fingers without releasing followed by a swipe
// down by a lesser amount which should still be enough to exit.
TEST_F(OverviewGestureHandlerTest, SwipeUpDownWithoutReleasing) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::EventGenerator generator(root_window, root_window);
  base::TimeDelta timestamp = base::TimeDelta::FromInternalValue(
      base::TimeTicks::Now().ToInternalValue());
  gfx::Point start;
  int num_fingers = 3;
  base::TimeDelta step_delay(base::TimeDelta::FromMilliseconds(5));
  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL,
                               start,
                               timestamp,
                               0,
                               0, 0,
                               0, 0,
                               num_fingers);
  generator.Dispatch(&fling_cancel);

  // Scroll up by 1000px.
  for (int i = 0; i < 100; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::ET_SCROLL,
                         start,
                         timestamp,
                         0,
                         0, -10,
                         0, -10,
                         num_fingers);
    generator.Dispatch(&move);
  }

  EXPECT_TRUE(IsSelecting());

  // Without releasing scroll back down by 600px.
  for (int i = 0; i < 60; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::ET_SCROLL,
                         start,
                         timestamp,
                         0,
                         0, 10,
                         0, 10,
                         num_fingers);
    generator.Dispatch(&move);
  }

  EXPECT_FALSE(IsSelecting());
  ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START,
                              start,
                              timestamp,
                              0,
                              0, 10,
                              0, 10,
                              num_fingers);
  generator.Dispatch(&fling_start);
}

// Tests a swipe down from the top of the screen to enter and exit overview.
TEST_F(OverviewGestureHandlerTest, GestureSwipe) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::EventGenerator generator(root_window, root_window);
  gfx::Point start_points[3];
  start_points[0] = start_points[1] = start_points[2] = gfx::Point();
  generator.GestureMultiFingerScroll(3, start_points, 5, 10, 0, 100);
  EXPECT_TRUE(IsSelecting());

  generator.GestureMultiFingerScroll(3, start_points, 5, 10, 0, 100);
  EXPECT_FALSE(IsSelecting());
}

// Tests that a swipe down from the top of a window doesn't enter overview.
// http://crbug.com/313859
TEST_F(OverviewGestureHandlerTest, GestureSwipeTopOfWindow) {
  gfx::Rect bounds(100, 100, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::EventGenerator generator(root_window, window2.get());
  gfx::Point start_points[3];
  start_points[0] = start_points[1] = start_points[2] = gfx::Point(105, 105);
  generator.GestureMultiFingerScroll(3, start_points, 5, 10, 0, 100);
  EXPECT_FALSE(IsSelecting());
}

}  // namespace ash
