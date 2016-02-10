// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/extended_mouse_warp_controller.h"

#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
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
  DisplayLayout layout(DisplayPlacement::RIGHT, 0);
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);

  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(359, 16, 1, 344),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(360, 0, 1, 360),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ(gfx::Rect(360, 16, 1, 344),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(359, 0, 1, 360),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  // Move 2nd display downwards a bit.
  layout.placement.offset = 5;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  // This is same as before because the 2nd display's y is above
  // the indicator's x.
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(359, 16, 1, 344),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(360, 5, 1, 355),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  EXPECT_EQ(gfx::Rect(360, 21, 1, 339),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(359, 5, 1, 355),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  // Move it down further so that the shared edge is shorter than
  // minimum hole size (160).
  layout.placement.offset = 200;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(359, 200, 1, 160),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(360, 200, 1, 160),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(360, 200, 1, 160),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(359, 200, 1, 160),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  // Now move 2nd display upwards
  layout.placement.offset = -5;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(359, 16, 1, 344),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(360, 0, 1, 360),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  // 16 px are reserved on 2nd display from top, so y must be
  // (16 - 5) = 11
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(360, 11, 1, 349),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(359, 0, 1, 360),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  event_filter()->HideSharedEdgeIndicator();
}

TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnLeft) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("360x360,700x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayLayout layout(DisplayPlacement::LEFT, 0);
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(0, 16, 1, 344),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(-1, 0, 1, 360),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(-1, 16, 1, 344),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 1, 360),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  layout.placement.offset = 250;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(0, 250, 1, 110),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(-1, 250, 1, 110),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(-1, 250, 1, 110),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(0, 250, 1, 110),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->HideSharedEdgeIndicator();
}

TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestOnTopBottom) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("360x360,700x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  DisplayLayout layout(DisplayPlacement::TOP, 0);
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(0, 0, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(0, -1, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(0, -1, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  layout.placement.offset = 250;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(250, 0, 110, 1),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(250, -1, 110, 1),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(250, -1, 110, 1),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(250, 0, 110, 1),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  layout.placement.position = DisplayPlacement::BOTTOM;
  layout.placement.offset = 0;
  display_manager->SetLayoutForCurrentDisplays(layout);
  event_filter()->ShowSharedEdgeIndicator(root_windows[0] /* primary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(0, 359, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(0, 360, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
  event_filter()->ShowSharedEdgeIndicator(root_windows[1] /* secondary */);
  ASSERT_EQ(1U, mouse_warp_controller()->warp_regions_.size());
  EXPECT_EQ(gfx::Rect(0, 360, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
  EXPECT_EQ(gfx::Rect(0, 359, 360, 1),
            mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());

  event_filter()->HideSharedEdgeIndicator();
}

// Verify indicators show up as expected with 3+ displays.
TEST_F(ExtendedMouseWarpControllerTest, IndicatorBoundsTestThreeDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  auto run_test = [this] {
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();

    // Left most display
    event_filter()->ShowSharedEdgeIndicator(root_windows[0]);
    ASSERT_EQ(2U, mouse_warp_controller()->warp_regions_.size());
    EXPECT_EQ(gfx::Rect(359, 16, 1, 344),
              mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
    EXPECT_EQ(gfx::Rect(360, 0, 1, 360),
              mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
    EXPECT_EQ(gfx::Rect(1060, 16, 1, 684),
              mouse_warp_controller()->warp_regions_[1]->a_indicator_bounds());
    EXPECT_EQ(gfx::Rect(1059, 0, 1, 700),
              mouse_warp_controller()->warp_regions_[1]->b_indicator_bounds());

    // Middle display
    event_filter()->ShowSharedEdgeIndicator(root_windows[1]);
    ASSERT_EQ(2U, mouse_warp_controller()->warp_regions_.size());
    EXPECT_EQ(gfx::Rect(360, 16, 1, 344),
              mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
    EXPECT_EQ(gfx::Rect(359, 0, 1, 360),
              mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
    EXPECT_EQ(gfx::Rect(1059, 16, 1, 684),
              mouse_warp_controller()->warp_regions_[1]->a_indicator_bounds());
    EXPECT_EQ(gfx::Rect(1060, 0, 1, 700),
              mouse_warp_controller()->warp_regions_[1]->b_indicator_bounds());

    // Right most display
    event_filter()->ShowSharedEdgeIndicator(root_windows[2]);
    ASSERT_EQ(2U, mouse_warp_controller()->warp_regions_.size());
    EXPECT_EQ(gfx::Rect(360, 16, 1, 344),
              mouse_warp_controller()->warp_regions_[0]->a_indicator_bounds());
    EXPECT_EQ(gfx::Rect(359, 0, 1, 360),
              mouse_warp_controller()->warp_regions_[0]->b_indicator_bounds());
    EXPECT_EQ(gfx::Rect(1060, 16, 1, 684),
              mouse_warp_controller()->warp_regions_[1]->a_indicator_bounds());
    EXPECT_EQ(gfx::Rect(1059, 0, 1, 700),
              mouse_warp_controller()->warp_regions_[1]->b_indicator_bounds());

    event_filter()->HideSharedEdgeIndicator();
  };

  UpdateDisplay("360x360,700x700,1000x1000");
  run_test();

  UpdateDisplay("360x360,700x700,1000x1000");
  test::SwapPrimaryDisplay();
  run_test();
}

}  // namespace ash
