// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/extended_mouse_warp_controller.h"

#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {

class ExtendedMouseWarpControllerTest : public test::AshTestBase {
 public:
  ExtendedMouseWarpControllerTest() {}
  ~ExtendedMouseWarpControllerTest() override {}

 protected:
  MouseCursorEventFilter* event_filter() {
    return Shell::GetInstance()->mouse_cursor_filter();
  }

  ExtendedMouseWarpController* mouse_warp_controller() {
    return static_cast<ExtendedMouseWarpController*>(
        event_filter()->mouse_warp_controller_for_test());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtendedMouseWarpControllerTest);
};

// Verifies if MouseCursorEventFilter's bounds calculation works correctly.
TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnRight) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("360x360,700x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayLayout layout(DisplayLayout::RIGHT, 0);
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("359,16 1x344",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,0 1x360",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("360,16 1x344",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,0 1x360",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  // Move 2nd display downwards a bit.
  layout.offset = 5;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  // This is same as before because the 2nd display's y is above
  // the indicator's x.
  EXPECT_EQ("359,16 1x344",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,5 1x355",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("360,21 1x339",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,5 1x355",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  // Move it down further so that the shared edge is shorter than
  // minimum hole size (160).
  layout.offset = 200;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("359,200 1x160",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,200 1x160",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("360,200 1x160",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,200 1x160",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  // Now move 2nd display upwards
  layout.offset = -5;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("359,16 1x344",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("360,0 1x360",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  // 16 px are reserved on 2nd display from top, so y must be
  // (16 - 5) = 11
  EXPECT_EQ("360,11 1x349",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("359,0 1x360",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  event_filter()->HideSharedEdgeIndicator();
}

TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnLeft) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("360x360,700x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayLayout layout(DisplayLayout::LEFT, 0);
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,16 1x344",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("-1,0 1x360",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("-1,16 1x344",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,0 1x360",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  layout.offset = 250;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,250 1x110",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("-1,250 1x110",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("-1,250 1x110",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,250 1x110",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->HideSharedEdgeIndicator();
}

TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnTopBottom) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("360x360,700x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayLayout layout(DisplayLayout::TOP, 0);
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,0 360x1",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,-1 360x1",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("0,-1 360x1",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,0 360x1",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  layout.offset = 250;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("250,0 110x1",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("250,-1 110x1",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("250,-1 110x1",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("250,0 110x1",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  layout.position = DisplayLayout::BOTTOM;
  layout.offset = 0;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  EXPECT_EQ("0,359 360x1",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,360 360x1",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ("0,360 360x1",
            mouse_warp_controller()->src_indicator_bounds_.ToString());
  EXPECT_EQ("0,359 360x1",
            mouse_warp_controller()->dst_indicator_bounds_.ToString());

  event_filter()->HideSharedEdgeIndicator();
}

}  // namespace ash
