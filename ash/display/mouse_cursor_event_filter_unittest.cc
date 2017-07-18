// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ui/aura/env.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class MouseCursorEventFilterTest : public test::AshTestBase {
 public:
  MouseCursorEventFilterTest() {}
  ~MouseCursorEventFilterTest() override {}

 protected:
  MouseCursorEventFilter* event_filter() {
    return Shell::Get()->mouse_cursor_filter();
  }

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_screen) {
    return test::AshTestBase::TestIfMouseWarpsAt(GetEventGenerator(),
                                                 point_in_screen);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseCursorEventFilterTest);
};

// Verifies if the mouse pointer correctly moves to another display when there
// are two displays.
TEST_F(MouseCursorEventFilterTest, WarpMouse) {
  UpdateDisplay("500x500,500x500");

  ASSERT_EQ(display::DisplayPlacement::RIGHT, Shell::Get()
                                                  ->display_manager()
                                                  ->GetCurrentDisplayLayout()
                                                  .placement_list[0]
                                                  .position);

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 11)));

  // Touch the right edge of the primary root window. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("501,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(500, 11)));
  EXPECT_EQ("498,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(0, 11)));
  // Touch the top edge of the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 0)));
  // Touch the bottom edge of the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 499)));
  // Touch the right edge of the secondary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(999, 11)));
  // Touch the top edge of the secondary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 0)));
  // Touch the bottom edge of the secondary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 499)));
}

// Verifies if the mouse pointer correctly moves to another display even when
// two displays are not the same size.
TEST_F(MouseCursorEventFilterTest, WarpMouseDifferentSizeDisplays) {
  UpdateDisplay("500x500,600x600");  // the second one is larger.

  ASSERT_EQ(display::DisplayPlacement::RIGHT, Shell::Get()
                                                  ->display_manager()
                                                  ->GetCurrentDisplayLayout()
                                                  .placement_list[0]
                                                  .position);

  // Touch the left edge of the secondary root window. Pointer should NOT warp
  // because 1px left of (0, 500) is outside the primary root window.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(501, 500)));
  EXPECT_EQ("501,500",
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp
  // because 1px left of (0, 480) is inside the primary root window.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(500, 480)));
  EXPECT_EQ("498,480",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies if the mouse pointer correctly moves between displays with
// different scale factors. In native coords mode, there is no
// difference between drag and move.
TEST_F(MouseCursorEventFilterTest, WarpMouseDifferentScaleDisplaysInNative) {
  UpdateDisplay("500x500,600x600*2");

  ASSERT_EQ(display::DisplayPlacement::RIGHT, Shell::Get()
                                                  ->display_manager()
                                                  ->GetCurrentDisplayLayout()
                                                  .placement_list[0]
                                                  .position);

  aura::Env::GetInstance()->set_last_mouse_location(gfx::Point(900, 123));

  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 123)));
  EXPECT_EQ("500,123",
            aura::Env::GetInstance()->last_mouse_location().ToString());
  // Touch the edge of 2nd display again and make sure it warps to
  // 1st dislay.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(500, 123)));
  // TODO(oshima): Due to a bug in EventGenerator, the screen coordinates
  // is shrinked by dsf once. Fix this.
  EXPECT_EQ("498,61",
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies if MouseCursorEventFilter::set_mouse_warp_enabled() works as
// expected.
TEST_F(MouseCursorEventFilterTest, SetMouseWarpModeFlag) {
  UpdateDisplay("500x500,500x500");

  event_filter()->set_mouse_warp_enabled(false);
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("499,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());

  event_filter()->set_mouse_warp_enabled(true);
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("501,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies cursor's device scale factor is updated when a cursor has moved
// across root windows with different device scale factors
// (http://crbug.com/154183).
TEST_F(MouseCursorEventFilterTest, CursorDeviceScaleFactor) {
  UpdateDisplay("400x400,800x800*2");
  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::RIGHT, 0));
  test::CursorManagerTestApi cursor_test_api(Shell::Get()->cursor_manager());

  EXPECT_EQ(1.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
  TestIfMouseWarpsAt(gfx::Point(399, 200));
  EXPECT_EQ(2.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
  TestIfMouseWarpsAt(gfx::Point(400, 200));
  EXPECT_EQ(1.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
}

}  // namespace ash
