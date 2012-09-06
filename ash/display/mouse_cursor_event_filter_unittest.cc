// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mouse_cursor_event_filter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/display/display_controller.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {
namespace {

gfx::Display GetPrimaryDisplay() {
  return gfx::Screen::GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[0]);
}

gfx::Display GetSecondaryDisplay() {
  return gfx::Screen::GetDisplayNearestWindow(
      Shell::GetAllRootWindows()[1]);
}

}  // namespace

typedef test::AshTestBase MouseCursorEventFilterTest;

// Verifies if the mouse pointer correctly moves to another display when there
// are two displays.
TEST_F(MouseCursorEventFilterTest, WarpMouse) {
  UpdateDisplay("500x500,500x500");

  MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  ASSERT_EQ(
      DisplayLayout::RIGHT,
      Shell::GetInstance()->
          display_controller()->default_display_layout().position);

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  bool is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                            gfx::Point(11, 11));
  EXPECT_FALSE(is_warped);
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                                       gfx::Point(11, 11));
  EXPECT_FALSE(is_warped);

  // Touch the right edge of the primary root window. Pointer should warp.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(499, 11));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("501,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                                       gfx::Point(0, 11));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("498,11",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the primary root window.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(0, 11));
  EXPECT_FALSE(is_warped);
  // Touch the top edge of the primary root window.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(11, 0));
  EXPECT_FALSE(is_warped);
  // Touch the bottom edge of the primary root window.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                     gfx::Point(11, 499));
  EXPECT_FALSE(is_warped);
  // Touch the right edge of the secondary root window.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(499, 11));
  EXPECT_FALSE(is_warped);
  // Touch the top edge of the secondary root window.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(11, 0));
  EXPECT_FALSE(is_warped);
  // Touch the bottom edge of the secondary root window.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(11, 499));
  EXPECT_FALSE(is_warped);
}

// Verifies if the mouse pointer correctly moves to another display even when
// two displays are not the same size.
TEST_F(MouseCursorEventFilterTest, WarpMouseDifferentSizeDisplays) {
  UpdateDisplay("500x500,600x600");  // the second one is larger.

  MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  ASSERT_EQ(
      DisplayLayout::RIGHT,
      Shell::GetInstance()->
          display_controller()->default_display_layout().position);

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::Env::GetInstance()->SetLastMouseLocation(*root_windows[1],
                                                 gfx::Point(123, 123));

  // Touch the left edge of the secondary root window. Pointer should NOT warp
  // because 1px left of (0, 500) is outside the primary root window.
  bool is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                                          gfx::Point(0, 500));
  EXPECT_FALSE(is_warped);
  EXPECT_EQ("623,123",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the secondary root window. Pointer should warp
  // because 1px left of (0, 499) is inside the primary root window.
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                                     gfx::Point(0, 499));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("498,499",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies if MouseCursorEventFilter::set_mouse_warp_mode() works as expected.
TEST_F(MouseCursorEventFilterTest, SetMouseWarpModeFlag) {
  UpdateDisplay("500x500,500x500");

  MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  aura::Env::GetInstance()->SetLastMouseLocation(*root_windows[0],
                                                 gfx::Point(1, 1));

  event_filter->set_mouse_warp_mode(MouseCursorEventFilter::WARP_NONE);
  bool is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                          gfx::Point(499, 11));
  EXPECT_FALSE(is_warped);
  EXPECT_EQ("1,1",
            aura::Env::GetInstance()->last_mouse_location().ToString());

  event_filter->set_mouse_warp_mode(MouseCursorEventFilter::WARP_ALWAYS);
  is_warped = event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                       gfx::Point(499, 11));
  EXPECT_TRUE(is_warped);
  EXPECT_EQ("501,11",
            aura::Env::GetInstance()->last_mouse_location().ToString());
}

// Verifies if MouseCursorEventFilter's bounds calculation works correctly.
TEST_F(MouseCursorEventFilterTest, IndicatorBoundsTestOnRight) {
  UpdateDisplay("360x360,700x700");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  DisplayController* controller =
      Shell::GetInstance()->display_controller();
  DisplayLayout default_layout(DisplayLayout::RIGHT, 0);
  controller->SetDefaultDisplayLayout(default_layout);
  ash::internal::MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("359,16 1x344", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,0 1x360", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("360,16 1x344", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,0 1x360", event_filter->dst_indicator_bounds_.ToString());

  // Move 2nd display downwards a bit.
  default_layout.offset = 5;
  controller->SetDefaultDisplayLayout(default_layout);
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  // This is same as before because the 2nd display's y is above
  // the indicator's x.
  EXPECT_EQ("359,16 1x344", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,5 1x355", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("360,21 1x339", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,5 1x355", event_filter->dst_indicator_bounds_.ToString());

  // Move it down further so that the shared edge is shorter than
  // minimum hole size (160).
  default_layout.offset = 200;
  controller->SetDefaultDisplayLayout(default_layout);
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("359,200 1x160", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,200 1x160", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("360,200 1x160", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,200 1x160", event_filter->dst_indicator_bounds_.ToString());

  // Now move 2nd display upwards
  default_layout.offset = -5;
  controller->SetDefaultDisplayLayout(default_layout);
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("359,16 1x344", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,0 1x360", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  // 16 px are reserved on 2nd display from top, so y must be
  // (16 - 5) = 11
  EXPECT_EQ("360,11 1x349", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,0 1x360", event_filter->dst_indicator_bounds_.ToString());

  event_filter->HideSharedEdgeIndicator();
}

TEST_F(MouseCursorEventFilterTest, IndicatorBoundsTestOnLeft) {
  UpdateDisplay("360x360,700x700");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  DisplayController* controller =
      Shell::GetInstance()->display_controller();
  DisplayLayout default_layout(DisplayLayout::LEFT, 0);
  controller->SetDefaultDisplayLayout(default_layout);
  ash::internal::MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,16 1x344", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("-1,0 1x360", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("-1,16 1x344", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,0 1x360", event_filter->dst_indicator_bounds_.ToString());

  default_layout.offset = 300;
  controller->SetDefaultDisplayLayout(default_layout);
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,300 1x60", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("-1,300 1x60", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("-1,300 1x60", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,300 1x60", event_filter->dst_indicator_bounds_.ToString());
  event_filter->HideSharedEdgeIndicator();
}

TEST_F(MouseCursorEventFilterTest, IndicatorBoundsTestOnTopBottom) {
  UpdateDisplay("360x360,700x700");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  DisplayController* controller =
      Shell::GetInstance()->display_controller();
  DisplayLayout default_layout(DisplayLayout::TOP, 0);
  controller->SetDefaultDisplayLayout(default_layout);
  ash::internal::MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,0 360x1", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,-1 360x1", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("0,-1 360x1", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,0 360x1", event_filter->dst_indicator_bounds_.ToString());

  default_layout.offset = 300;
  controller->SetDefaultDisplayLayout(default_layout);
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("300,0 60x1", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("300,-1 60x1", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("300,-1 60x1", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("300,0 60x1", event_filter->dst_indicator_bounds_.ToString());

  default_layout.position = DisplayLayout::BOTTOM;
  default_layout.offset = 0;
  controller->SetDefaultDisplayLayout(default_layout);
  event_filter->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,359 360x1", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,360 360x1", event_filter->dst_indicator_bounds_.ToString());
  event_filter->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("0,360 360x1", event_filter->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,359 360x1", event_filter->dst_indicator_bounds_.ToString());

  event_filter->HideSharedEdgeIndicator();
}

}  // namespace internal
}  // namespace ash
