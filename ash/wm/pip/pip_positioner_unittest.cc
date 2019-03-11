// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/root_window_controller.h"
#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

display::Display GetDisplayForWindow(aura::Window* window) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window);
}

gfx::Rect ConvertToScreenForWindow(aura::Window* window,
                                   const gfx::Rect& bounds) {
  gfx::Rect new_bounds = bounds;
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &new_bounds);
  return new_bounds;
}

void ForceHideShelves() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    auto* shelf = root_window_controller->shelf();
    auto* layout_manager = shelf->shelf_layout_manager();
    shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
    layout_manager->LayoutShelf();  // Force layout to end animation.
  }
}

gfx::Rect ConvertPrimaryToScreen(const gfx::Rect& bounds) {
  return ConvertToScreenForWindow(Shell::GetPrimaryRootWindow(), bounds);
}

}  // namespace

using PipPositionerTest = AshTestBase;

TEST_F(PipPositionerTest,
       PipRestingPositionSnapsInDisplayWithLargeAspectRatio) {
  UpdateDisplay("1600x400");

  // Snap to the top edge instead of the far left edge.
  EXPECT_EQ(ConvertPrimaryToScreen(gfx::Rect(500, 8, 100, 100)),
            PipPositioner::GetRestingPosition(
                GetPrimaryDisplay(),
                ConvertPrimaryToScreen(gfx::Rect(500, 100, 100, 100))));
}

TEST_F(PipPositionerTest, AvoidObstaclesAvoidsUnifiedSystemTray) {
  UpdateDisplay("1000x1000");
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble(/*show_by_click=*/false);

  auto display = GetPrimaryDisplay();
  gfx::Rect area = PipPositioner::GetMovementArea(display);
  gfx::Rect bubble_bounds = unified_system_tray->GetBubbleBoundsInScreen();
  gfx::Rect bounds = gfx::Rect(bubble_bounds.x(), bubble_bounds.y(), 100, 100);
  gfx::Rect moved_bounds = PipPositioner::GetRestingPosition(display, bounds);

  // Expect that the returned bounds don't intersect the unified system tray
  // but also don't leave the PIP movement area.
  EXPECT_FALSE(moved_bounds.Intersects(bubble_bounds));
  EXPECT_TRUE(area.Contains(moved_bounds));
}

class PipPositionerDisplayTest : public AshTestBase,
                                 public ::testing::WithParamInterface<
                                     std::tuple<std::string, std::size_t>> {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();

    const std::string& display_string = std::get<0>(GetParam());
    const std::size_t root_window_index = std::get<1>(GetParam());
    UpdateWorkArea(display_string);
    ASSERT_LT(root_window_index, Shell::GetAllRootWindows().size());
    root_window_ = Shell::GetAllRootWindows()[root_window_index];
    scoped_root_.reset(new ScopedRootWindowForNewWindows(root_window_));
  }

  void TearDown() override {
    scoped_root_.reset();
    AshTestBase::TearDown();
  }

 protected:
  display::Display GetDisplay() { return GetDisplayForWindow(root_window_); }

  gfx::Rect ConvertToScreen(const gfx::Rect& bounds) {
    return ConvertToScreenForWindow(root_window_, bounds);
  }

  gfx::Rect CallAvoidObstacles(const display::Display& display,
                               gfx::Rect bounds) {
    return PipPositioner::AvoidObstacles(display, bounds);
  }

  // TODO dedpue?
  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    for (aura::Window* root : Shell::GetAllRootWindows())
      Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
  }

 private:
  std::unique_ptr<ScopedRootWindowForNewWindows> scoped_root_;
  aura::Window* root_window_;
};

TEST_P(PipPositionerDisplayTest, PipMovementAreaIsInset) {
  gfx::Rect area = PipPositioner::GetMovementArea(GetDisplay());
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 8, 384, 384)), area);
}

TEST_P(PipPositionerDisplayTest,
       PipMovementAreaIncludesKeyboardIfKeyboardIsShown) {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboardInDisplay(GetDisplay());
  ASSERT_TRUE(keyboard::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  gfx::Rect area = PipPositioner::GetMovementArea(GetDisplay());
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 8, 384, 284)), area);
}

TEST_P(PipPositionerDisplayTest, PipRestingPositionSnapsToClosestEdge) {
  auto display = GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, 50, 100, 100))));

  // Snap near bottom edge to bottom.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 292, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, 250, 100, 100))));

  // Snap near left edge to left.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 100, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(50, 100, 100, 100))));

  // Snap near right edge to right.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(292, 100, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(250, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, PipRestingPositionSnapsInsideDisplay) {
  auto display = GetDisplay();

  // Snap near top edge outside movement area to top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, -50, 100, 100))));

  // Snap near bottom edge outside movement area to bottom.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 292, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, 450, 100, 100))));

  // Snap near left edge outside movement area to left.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 100, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(-50, 100, 100, 100))));

  // Snap near right edge outside movement area to right.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(292, 100, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(450, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, PipAdjustPositionForDragClampsToMovementArea) {
  auto display = GetDisplay();

  // Adjust near top edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(100, -50, 100, 100))));

  // Adjust near bottom edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 292, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(100, 450, 100, 100))));

  // Adjust near left edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 100, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(-50, 100, 100, 100))));

  // Adjust near right edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(292, 100, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(450, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, PipRestingPositionWorksIfKeyboardIsDisabled) {
  SetTouchKeyboardEnabled(false);
  auto display = GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(100, 50, 100, 100))));
}

TEST_P(PipPositionerDisplayTest,
       PipDismissedPositionDoesNotMoveAnExcessiveDistance) {
  auto display = GetDisplay();

  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, PipDismissedPositionChosesClosestEdge) {
  auto display = GetDisplay();

  // Dismiss near top edge outside movement area towards top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, -100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 50, 100, 100))));

  // Dismiss near bottom edge outside movement area towards bottom.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 400, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 250, 100, 100))));

  // Dismiss near left edge outside movement area towards left.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-100, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(50, 100, 100, 100))));

  // Dismiss near right edge outside movement area towards right.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(400, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(250, 100, 100, 100))));
}

// Verify that if two edges are equally close, the PIP window prefers dismissing
// out horizontally.
TEST_P(PipPositionerDisplayTest, PipDismissedPositionPrefersHorizontal) {
  auto display = GetDisplay();

  // Top left corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-150, 0, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(0, 0, 100, 100))));

  // Top right corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(450, 0, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(300, 0, 100, 100))));

  // Bottom left corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-150, 300, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(0, 300, 100, 100))));

  // Bottom right corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(450, 300, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(300, 300, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, AvoidObstaclesAvoidsFloatingKeyboard) {
  auto display = GetDisplay();

  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->SetContainerType(
      keyboard::mojom::ContainerType::kFloating, base::nullopt,
      base::DoNothing());
  keyboard_controller->ShowKeyboardInDisplay(display);
  ASSERT_TRUE(keyboard::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 0, 100, 100));
  ForceHideShelves();  // Showing the keyboard may have shown the shelf.

  gfx::Rect area = PipPositioner::GetMovementArea(display);
  gfx::Rect moved_bounds =
      CallAvoidObstacles(display, ConvertToScreen(gfx::Rect(8, 8, 100, 100)));

  // Expect that the returned bounds don't intersect the floating keyboard
  // but also don't leave the PIP movement area.
  EXPECT_FALSE(moved_bounds.Intersects(keyboard_window->GetBoundsInScreen()));
  EXPECT_TRUE(area.Contains(moved_bounds));
}

TEST_P(PipPositionerDisplayTest,
       AvoidObstaclesDoesNotChangeBoundsIfThereIsNoCollision) {
  auto display = GetDisplay();
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 100, 100, 100)),
            CallAvoidObstacles(display,
                               ConvertToScreen(gfx::Rect(100, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, GetRestingPositionAvoidsKeyboard) {
  auto display = GetDisplay();

  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboardInDisplay(display);
  ASSERT_TRUE(keyboard::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 192, 100, 100)),
            PipPositioner::GetRestingPosition(
                display, ConvertToScreen(gfx::Rect(8, 300, 100, 100))));
}

// TODO: UpdateDisplay() doesn't support different layouts of multiple displays.
// We should add some way to try multiple layouts.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PipPositionerDisplayTest,
    testing::Values(std::make_tuple("400x400", 0u),
                    std::make_tuple("400x400/r", 0u),
                    std::make_tuple("400x400/u", 0u),
                    std::make_tuple("400x400/l", 0u),
                    std::make_tuple("800x800*2", 0u),
                    std::make_tuple("400x400,400x400", 0u),
                    std::make_tuple("400x400,400x400", 1u)));

class PipPositionerLogicTest : public ::testing::Test {
 public:
  gfx::Rect CallAvoidObstaclesInternal(const gfx::Rect& work_area,
                                       const std::vector<gfx::Rect>& rects,
                                       const gfx::Rect& bounds) {
    return PipPositioner::AvoidObstaclesInternal(work_area, rects, bounds);
  }
};

TEST_F(PipPositionerLogicTest,
       AvoidObstaclesDoesNotMoveBoundsIfThereIsNoIntersection) {
  const gfx::Rect area(0, 0, 400, 400);

  // Check no collision with Rect.
  EXPECT_EQ(gfx::Rect(200, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(0, 0, 100, 100)},
                                       gfx::Rect(200, 0, 100, 100)));

  // Check no collision with edges of the work area. Provide an obstacle so
  // it has something to stick to, to distinguish failure from correctly
  // not moving the PIP bounds.
  // Check corners:
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(300, 300, 1, 1)},
                                       gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(gfx::Rect(300, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(300, 0, 100, 100)));
  EXPECT_EQ(gfx::Rect(0, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(0, 300, 100, 100)));
  EXPECT_EQ(gfx::Rect(300, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(300, 300, 100, 100)));

  // Check edges:
  EXPECT_EQ(gfx::Rect(100, 0, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(100, 0, 100, 100)));
  EXPECT_EQ(gfx::Rect(0, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(0, 100, 100, 100)));
  EXPECT_EQ(gfx::Rect(300, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(300, 100, 100, 100)));
  EXPECT_EQ(gfx::Rect(100, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(100, 300, 100, 100)));
}

TEST_F(PipPositionerLogicTest, AvoidObstaclesOffByOneCases) {
  const gfx::Rect area(0, 0, 400, 400);

  // Test 1x1 PIP window intersecting a 1x1 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(10, 10, 1, 1)));
  // Test 1x1 PIP window adjacent to a 1x1 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(9, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(11, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(10, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(10, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(9, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(11, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(11, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 1, 1)},
                                       gfx::Rect(9, 11, 1, 1)));

  // Test 1x1 PIP window intersecting a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 11, 1, 1)));

  // Test 1x1 PIP window adjacent to a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(9, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 12, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 12, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 11, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 11, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 10, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 10, 1, 1)));
  EXPECT_EQ(gfx::Rect(12, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(11, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(10, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(10, 9, 1, 1)));
  EXPECT_EQ(gfx::Rect(9, 9, 1, 1),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 9, 1, 1)));

  // Test 2x2 PIP window intersecting a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(8, 9, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 9, 2, 2)));
  EXPECT_EQ(gfx::Rect(12, 9, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 9, 2, 2)));
  EXPECT_EQ(gfx::Rect(12, 11, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 11, 2, 2)));
  EXPECT_EQ(gfx::Rect(8, 11, 2, 2),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 11, 2, 2)));

  // Test 3x3 PIP window intersecting a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(7, 8, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(8, 8, 3, 3)));
  EXPECT_EQ(gfx::Rect(12, 8, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 8, 3, 3)));
  EXPECT_EQ(gfx::Rect(12, 11, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(11, 11, 3, 3)));
  EXPECT_EQ(gfx::Rect(7, 11, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(8, 11, 3, 3)));

  // Test 3x3 PIP window adjacent to a 2x2 obstacle.
  EXPECT_EQ(gfx::Rect(7, 10, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(7, 10, 3, 3)));
  EXPECT_EQ(gfx::Rect(12, 10, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(12, 10, 3, 3)));
  EXPECT_EQ(gfx::Rect(9, 7, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 7, 3, 3)));
  EXPECT_EQ(gfx::Rect(9, 12, 3, 3),
            CallAvoidObstaclesInternal(area, {gfx::Rect(10, 10, 2, 2)},
                                       gfx::Rect(9, 12, 3, 3)));
}

TEST_F(PipPositionerLogicTest, AvoidObstaclesNestedObstacle) {
  const gfx::Rect area(0, 0, 400, 400);
  EXPECT_EQ(gfx::Rect(9, 16, 1, 1),
            CallAvoidObstaclesInternal(
                area, {gfx::Rect(15, 15, 5, 5), gfx::Rect(10, 10, 15, 15)},
                gfx::Rect(16, 16, 1, 1)));
}

TEST_F(PipPositionerLogicTest, AvoidObstaclesAvoidsTwoObstacles) {
  const gfx::Rect area(0, 0, 400, 400);
  const std::vector<gfx::Rect> obstacles = {gfx::Rect(4, 1, 4, 5),
                                            gfx::Rect(2, 4, 4, 5)};

  // Test a 2x2 PIP window in the intersection between the obstacles.
  EXPECT_EQ(gfx::Rect(2, 2, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(4, 4, 2, 2)));
  // Test a 2x2 PIP window in the lower obstacle.
  EXPECT_EQ(gfx::Rect(0, 7, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(2, 7, 2, 2)));
  // Test a 2x2 PIP window in the upper obstacle.
  EXPECT_EQ(gfx::Rect(2, 1, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(4, 1, 2, 2)));
}

TEST_F(PipPositionerLogicTest, AvoidObstaclesAvoidsThreeObstacles) {
  const gfx::Rect area(0, 0, 400, 400);
  const std::vector<gfx::Rect> obstacles = {
      gfx::Rect(4, 1, 4, 5), gfx::Rect(2, 4, 4, 5), gfx::Rect(2, 1, 3, 4)};

  // Test a 2x2 PIP window intersecting the top two obstacles.
  EXPECT_EQ(gfx::Rect(0, 2, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(3, 2, 2, 2)));
  // Test a 2x2 PIP window intersecting all three obstacles.
  EXPECT_EQ(gfx::Rect(0, 3, 2, 2),
            CallAvoidObstaclesInternal(area, obstacles, gfx::Rect(3, 3, 2, 2)));
}

TEST_F(PipPositionerLogicTest,
       AvoidObstaclesDoesNotPositionBoundsOutsideOfPipArea) {
  // Position the bounds such that moving it the least distance to stop
  // intersecting |obstacle| would put it outside of |area|. It should go
  // instead to the position of second least distance, which would be below
  // |obstacle|.
  const gfx::Rect area(0, 0, 400, 400);
  const gfx::Rect obstacle(50, 0, 100, 100);
  const gfx::Rect bounds(25, 0, 100, 100);
  EXPECT_EQ(gfx::Rect(25, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle}, bounds));
}

TEST_F(PipPositionerLogicTest,
       AvoidObstaclesPositionsBoundsWithLeastDisplacement) {
  const gfx::Rect area(0, 0, 400, 400);
  const gfx::Rect obstacle(200, 200, 100, 100);

  // Intersecting slightly on the left.
  EXPECT_EQ(gfx::Rect(100, 200, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(150, 200, 100, 100)));

  // Intersecting slightly on the right.
  EXPECT_EQ(gfx::Rect(300, 200, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(250, 200, 100, 100)));

  // Intersecting slightly on the bottom.
  EXPECT_EQ(gfx::Rect(200, 300, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(200, 250, 100, 100)));

  // Intersecting slightly on the top.
  EXPECT_EQ(gfx::Rect(200, 100, 100, 100),
            CallAvoidObstaclesInternal(area, {obstacle},
                                       gfx::Rect(200, 150, 100, 100)));
}

}  // namespace ash
