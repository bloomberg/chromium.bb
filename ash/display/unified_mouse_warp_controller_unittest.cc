// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/unified_mouse_warp_controller.h"

#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

class UnifiedMouseWarpControllerTest : public test::AshTestBase {
 public:
  UnifiedMouseWarpControllerTest() {}
  ~UnifiedMouseWarpControllerTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    Shell::GetInstance()->display_manager()->SetUnifiedDesktopEnabled(true);
  }

 protected:
  bool FindMirrroingDisplayIdContainingNativePoint(
      const gfx::Point& point_in_native,
      int64_t* display_id,
      gfx::Point* point_in_mirroring_host,
      gfx::Point* point_in_unified_host) {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    for (auto display : display_manager->software_mirroring_display_list()) {
      DisplayInfo info = display_manager->GetDisplayInfo(display.id());
      if (info.bounds_in_native().Contains(point_in_native)) {
        *display_id = info.id();
        *point_in_unified_host = point_in_native;
        const gfx::Point& origin = info.bounds_in_native().origin();
        // Convert to mirroring host.
        point_in_unified_host->Offset(-origin.x(), -origin.y());
        *point_in_mirroring_host = *point_in_unified_host;
        // Convert from mirroring host to unified host.
        AshWindowTreeHost* ash_host =
            Shell::GetInstance()
                ->window_tree_host_manager()
                ->mirror_window_controller()
                ->GetAshWindowTreeHostForDisplayId(info.id());
        ash_host->AsWindowTreeHost()->ConvertPointFromHost(
            point_in_unified_host);
        return true;
      }
    }
    return false;
  }

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_native) {
    static_cast<UnifiedMouseWarpController*>(
        Shell::GetInstance()
            ->mouse_cursor_filter()
            ->mouse_warp_controller_for_test())
        ->update_location_for_test();
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    int64_t orig_mirroring_display_id;
    gfx::Point point_in_unified_host;
    gfx::Point point_in_mirroring_host;
    if (!FindMirrroingDisplayIdContainingNativePoint(
            point_in_native, &orig_mirroring_display_id,
            &point_in_mirroring_host, &point_in_unified_host)) {
      return false;
    }
#if defined(USE_OZONE)
    // The location of the ozone's native event is relative to the host.
    GetEventGenerator().MoveMouseToWithNative(point_in_unified_host,
                                              point_in_mirroring_host);
#else
    GetEventGenerator().MoveMouseToWithNative(point_in_unified_host,
                                              point_in_native);
#endif
    aura::Window* root = Shell::GetPrimaryRootWindow();
    gfx::Point new_location_in_unified_host =
        aura::Env::GetInstance()->last_mouse_location();
    // Convert screen to the host.
    root->GetHost()->ConvertPointToHost(&new_location_in_unified_host);

    int new_index = FindDisplayIndexContainingPoint(
        display_manager->software_mirroring_display_list(),
        new_location_in_unified_host);
    if (new_index < 0)
      return false;
    return orig_mirroring_display_id !=
           display_manager->software_mirroring_display_list()[new_index].id();
  }

  MouseCursorEventFilter* event_filter() {
    return Shell::GetInstance()->mouse_cursor_filter();
  }

  UnifiedMouseWarpController* mouse_warp_controller() {
    return static_cast<UnifiedMouseWarpController*>(
        event_filter()->mouse_warp_controller_for_test());
  }

  void BoundaryTestBody(const std::string& displays_with_same_height,
                        const std::string& displays_with_different_heights) {
    UpdateDisplay(displays_with_same_height);
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    // Let the UnifiedMouseWarpController compute the bounds by
    // generating a mouse move event.
    GetEventGenerator().MoveMouseTo(gfx::Point(0, 0));
    EXPECT_EQ("399,0 1x400",
              mouse_warp_controller()->first_edge_bounds_in_native_.ToString());
    EXPECT_EQ(
        "0,450 1x400",
        mouse_warp_controller()->second_edge_bounds_in_native_.ToString());

    // Scaled.
    UpdateDisplay(displays_with_different_heights);
    root_windows = Shell::GetAllRootWindows();
    // Let the UnifiedMouseWarpController compute the bounds by
    // generating a mouse move event.
    GetEventGenerator().MoveMouseTo(gfx::Point(1, 1));

    EXPECT_EQ("399,0 1x400",
              mouse_warp_controller()->first_edge_bounds_in_native_.ToString());
    EXPECT_EQ(
        "0,450 1x600",
        mouse_warp_controller()->second_edge_bounds_in_native_.ToString());
  }

  void NoWarpTestBody() {
    // Touch the left edge of the first display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(0, 10)));
    // Touch the top edge of the first display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 0)));
    // Touch the bottom edge of the first display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 499)));

    // Touch the right edge of the second display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(1099, 10)));
    // Touch the top edge of the second display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(610, 0)));
    // Touch the bottom edge of the second display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(610, 499)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnifiedMouseWarpControllerTest);
};

// Verifies if MouseCursorEventFilter's bounds calculation works correctly.
TEST_F(UnifiedMouseWarpControllerTest, BoundaryTest) {
  if (!SupportsMultipleDisplays())
    return;

  {
    SCOPED_TRACE("1x1");
    BoundaryTestBody("400x400,0+450-700x400", "400x400,0+450-700x600");
  }
  {
    SCOPED_TRACE("2x1");
    BoundaryTestBody("400x400*2,0+450-700x400", "400x400*2,0+450-700x600");
  }
  {
    SCOPED_TRACE("1x2");
    BoundaryTestBody("400x400,0+450-700x400*2", "400x400,0+450-700x600*2");
  }
  {
    SCOPED_TRACE("2x2");
    BoundaryTestBody("400x400*2,0+450-700x400*2", "400x400*2,0+450-700x600*2");
  }
}

// Verifies if the mouse pointer correctly moves to another display in
// unified desktop mode.
TEST_F(UnifiedMouseWarpControllerTest, WarpMouse) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("500x500,600+0-500x500");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 10)));
  EXPECT_EQ("501,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(600, 10)));
  EXPECT_EQ("498,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
  {
    SCOPED_TRACE("1x1 NO WARP");
    NoWarpTestBody();
  }

  // With 2X and 1X displays
  UpdateDisplay("500x500*2,600+0-500x500");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 10)));
  EXPECT_EQ("250,5",  // moved to 501 by 2px, devided by 2 (dsf).
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(600, 10)));
  EXPECT_EQ("249,5",  // moved to 498 by 2px, divided by 2 (dsf).
            aura::Env::GetInstance()->last_mouse_location().ToString());

  {
    SCOPED_TRACE("2x1 NO WARP");
    NoWarpTestBody();
  }

  // With 1X and 2X displays
  UpdateDisplay("500x500,600+0-500x500*2");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 10)));
  EXPECT_EQ("501,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(600, 10)));
  EXPECT_EQ("498,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
  {
    SCOPED_TRACE("1x2 NO WARP");
    NoWarpTestBody();
  }

  // With two 2X displays
  UpdateDisplay("500x500*2,600+0-500x500*2");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(499, 10)));
  EXPECT_EQ("250,5",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(600, 10)));
  EXPECT_EQ("249,5",  // moved to 498 by 2px, divided by 2 (dsf).
            aura::Env::GetInstance()->last_mouse_location().ToString());
  {
    SCOPED_TRACE("1x2 NO WARP");
    NoWarpTestBody();
  }
}

}  // namespace aura
