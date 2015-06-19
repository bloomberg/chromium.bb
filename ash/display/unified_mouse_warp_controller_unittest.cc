// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/unified_mouse_warp_controller.h"

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ui/aura/env.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {

class UnifiedMouseWarpControllerTest : public test::AshTestBase {
 public:
  UnifiedMouseWarpControllerTest() {}
  ~UnifiedMouseWarpControllerTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    test::DisplayManagerTestApi::EnableUnifiedDesktopForTest();
  }

 protected:
  bool TestIfMouseWarpsAt(const gfx::Point& point_in_screen) {
    return test::DisplayManagerTestApi::TestIfMouseWarpsAt(GetEventGenerator(),
                                                           point_in_screen);
  }

  MouseCursorEventFilter* event_filter() {
    return Shell::GetInstance()->mouse_cursor_filter();
  }

  UnifiedMouseWarpController* mouse_warp_controller() {
    return static_cast<UnifiedMouseWarpController*>(
        event_filter()->mouse_warp_controller_for_test());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnifiedMouseWarpControllerTest);
};

// Verifies if MouseCursorEventFilter's bounds calculation works correctly.
TEST_F(UnifiedMouseWarpControllerTest, BoundaryTest) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,0+450-700x500");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  // Let the UnifiedMouseWarpController compute the bounds by
  // generating a mouse move event.
  GetEventGenerator().MoveMouseTo(gfx::Point(0, 0));

  EXPECT_EQ("399,0 1x400",
            mouse_warp_controller()->first_edge_bounds_in_native_.ToString());
  EXPECT_EQ("0,450 1x400",
            mouse_warp_controller()->second_edge_bounds_in_native_.ToString());
}

// Verifies if the mouse pointer correctly moves to another display in
// unified desktop mode.
TEST_F(UnifiedMouseWarpControllerTest, WarpMouse) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x500,500x500");
  ASSERT_EQ(1, gfx::Screen::GetScreenFor(Shell::GetPrimaryRootWindow())
                   ->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 11)));

  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 11)));
  EXPECT_EQ("501,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(500, 11)));
  EXPECT_EQ("498,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the first display.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(0, 11)));
  // Touch the top edge of the first display.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 0)));
  // Touch the bottom edge of the first display.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 499)));
  // Touch the right edge of the second display.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(999, 11)));
  // Touch the top edge of the second display.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 0)));
  // Touch the bottom edge of the second display.
  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(11, 499)));
}

}  // namespace aura
