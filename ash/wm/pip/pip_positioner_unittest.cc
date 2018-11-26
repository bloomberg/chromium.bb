// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace ash {

namespace {

// WindowState based on a given initial state.
class FakeWindowState : public wm::WindowState::State {
 public:
  explicit FakeWindowState(mojom::WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}
  ~FakeWindowState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(wm::WindowState* window_state,
                 const wm::WMEvent* event) override {}
  mojom::WindowStateType GetType() const override { return state_type_; }
  void AttachState(wm::WindowState* window_state,
                   wm::WindowState::State* previous_state) override {}
  void DetachState(wm::WindowState* window_state) override {}

 private:
  mojom::WindowStateType state_type_;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowState);
};

}  // namespace

class PipPositionerTest : public AshTestBase {
 public:
  PipPositionerTest() = default;
  ~PipPositionerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    SetTouchKeyboardEnabled(true);
    Shell::Get()->EnableKeyboard();

    UpdateWorkArea("400x400");
    window_ = CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100));
    wm::WindowState* window_state = wm::GetWindowState(window_);
    test_state_ = new FakeWindowState(mojom::WindowStateType::PIP);
    window_state->SetStateObject(
        std::unique_ptr<wm::WindowState::State>(test_state_));
  }

  void TearDown() override {
    SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    aura::Window* root = Shell::GetPrimaryRootWindow();
    Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
  }

  gfx::Rect CallAvoidObstacles(const display::Display& display,
                               gfx::Rect bounds) {
    return PipPositioner::AvoidObstacles(display, bounds);
  }

  gfx::Rect CallAvoidObstaclesInternal(const gfx::Rect& work_area,
                                       const std::vector<gfx::Rect>& rects,
                                       const gfx::Rect& bounds) {
    return PipPositioner::AvoidObstaclesInternal(work_area, rects, bounds);
  }

 protected:
  aura::Window* window() { return window_; }
  wm::WindowState* window_state() { return wm::GetWindowState(window_); }
  FakeWindowState* test_state() { return test_state_; }

 private:
  aura::Window* window_;
  FakeWindowState* test_state_;

  DISALLOW_COPY_AND_ASSIGN(PipPositionerTest);
};

TEST_F(PipPositionerTest, PipMovementAreaIsInset) {
  gfx::Rect area = PipPositioner::GetMovementArea(window_state()->GetDisplay());
  EXPECT_EQ(gfx::Rect(8, 8, 384, 384), area);
}

TEST_F(PipPositionerTest, PipMovementAreaIncludesKeyboardIfKeyboardIsShown) {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));
  ASSERT_TRUE(keyboard::WaitUntilShown());

  gfx::Rect area = PipPositioner::GetMovementArea(window_state()->GetDisplay());
  EXPECT_EQ(gfx::Rect(8, 8, 384, 284 - ShelfConstants::shelf_size()), area);
}

TEST_F(PipPositionerTest, PipRestingPositionSnapsToClosestEdge) {
  auto display = window_state()->GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ(
      gfx::Rect(100, 8, 100, 100),
      PipPositioner::GetRestingPosition(display, gfx::Rect(100, 50, 100, 100)));

  // Snap near bottom edge to bottom.
  EXPECT_EQ(gfx::Rect(100, 292, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(100, 250, 100, 100)));

  // Snap near left edge to left.
  EXPECT_EQ(
      gfx::Rect(8, 100, 100, 100),
      PipPositioner::GetRestingPosition(display, gfx::Rect(50, 100, 100, 100)));

  // Snap near right edge to right.
  EXPECT_EQ(gfx::Rect(292, 100, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(250, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipRestingPositionSnapsInsideDisplay) {
  auto display = window_state()->GetDisplay();

  // Snap near top edge outside movement area to top.
  EXPECT_EQ(gfx::Rect(100, 8, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(100, -50, 100, 100)));

  // Snap near bottom edge outside movement area to bottom.
  EXPECT_EQ(gfx::Rect(100, 292, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(100, 450, 100, 100)));

  // Snap near left edge outside movement area to left.
  EXPECT_EQ(gfx::Rect(8, 100, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(-50, 100, 100, 100)));

  // Snap near right edge outside movement area to right.
  EXPECT_EQ(gfx::Rect(292, 100, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(450, 100, 100, 100)));
}

TEST_F(PipPositionerTest,
       PipRestingPositionSnapsInDisplayWithLargeAspectRatio) {
  UpdateDisplay("1600x400");
  auto display = window_state()->GetDisplay();

  // Snap to the top edge instead of the far left edge.
  EXPECT_EQ(gfx::Rect(500, 8, 100, 100),
            PipPositioner::GetRestingPosition(display,
                                              gfx::Rect(500, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipAdjustPositionForDragClampsToMovementArea) {
  auto display = window_state()->GetDisplay();

  // Adjust near top edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(100, 8, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(100, -50, 100, 100)));

  // Adjust near bottom edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(100, 292, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(100, 450, 100, 100)));

  // Adjust near left edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(8, 100, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(-50, 100, 100, 100)));

  // Adjust near right edge outside movement area.
  EXPECT_EQ(
      gfx::Rect(292, 100, 100, 100),
      PipPositioner::GetBoundsForDrag(display, gfx::Rect(450, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipRestingPositionWorksIfKeyboardIsDisabled) {
  Shell::Get()->DisableKeyboard();
  auto display = window_state()->GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ(
      gfx::Rect(100, 8, 100, 100),
      PipPositioner::GetRestingPosition(display, gfx::Rect(100, 50, 100, 100)));
}

TEST_F(PipPositionerTest, PipDismissedPositionDoesNotMoveAnExcessiveDistance) {
  auto display = window_state()->GetDisplay();

  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(100, 100, 100, 100)));
}

TEST_F(PipPositionerTest, PipDismissedPositionChosesClosestEdge) {
  auto display = window_state()->GetDisplay();

  // Dismiss near top edge outside movement area towards top.
  EXPECT_EQ(gfx::Rect(100, -100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(100, 50, 100, 100)));

  // Dismiss near bottom edge outside movement area towards bottom.
  EXPECT_EQ(gfx::Rect(100, 400, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(100, 250, 100, 100)));

  // Dismiss near left edge outside movement area towards left.
  EXPECT_EQ(gfx::Rect(-100, 100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(50, 100, 100, 100)));

  // Dismiss near right edge outside movement area towards right.
  EXPECT_EQ(gfx::Rect(400, 100, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(250, 100, 100, 100)));
}

// Verify that if two edges are equally close, the PIP window prefers dismissing
// out horizontally.
TEST_F(PipPositionerTest, PipDismissedPositionPrefersHorizontal) {
  auto display = window_state()->GetDisplay();

  // Top left corner.
  EXPECT_EQ(
      gfx::Rect(-150, 0, 100, 100),
      PipPositioner::GetDismissedPosition(display, gfx::Rect(0, 0, 100, 100)));

  // Top right corner.
  EXPECT_EQ(gfx::Rect(450, 0, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(300, 0, 100, 100)));

  // Bottom left corner.
  EXPECT_EQ(gfx::Rect(-150, 300, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(0, 300, 100, 100)));

  // Bottom right corner.
  EXPECT_EQ(gfx::Rect(450, 300, 100, 100),
            PipPositioner::GetDismissedPosition(display,
                                                gfx::Rect(300, 300, 100, 100)));
}

TEST_F(PipPositionerTest,
       PipRestoresToPreviousBoundsOnMovementAreaChangeIfTheyExist) {
  // Position the PIP window on the side of the screen where it will be next
  // to an edge and therefore in a resting position for the whole test.
  const gfx::Rect bounds = gfx::Rect(292, 200, 100, 100);
  // Set restore position to where the window currently is.
  window()->SetBounds(bounds);
  window_state()->SetRestoreBoundsInScreen(bounds);
  EXPECT_TRUE(window_state()->HasRestoreBounds());

  // Update the work area so that the PIP window should be pushed upward.
  UpdateWorkArea("400x200");

  // Set PIP to the updated constrained bounds.
  const gfx::Rect constrained_bounds =
      PipPositioner::GetPositionAfterMovementAreaChange(window_state());
  EXPECT_EQ(gfx::Rect(292, 92, 100, 100), constrained_bounds);
  window()->SetBounds(constrained_bounds);

  // Restore the original work area.
  UpdateWorkArea("400x400");

  // Expect that the PIP window is put back to where it was before.
  EXPECT_EQ(gfx::Rect(292, 200, 100, 100),
            PipPositioner::GetPositionAfterMovementAreaChange(window_state()));
}

TEST_F(PipPositionerTest,
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

TEST_F(PipPositionerTest, AvoidObstaclesOffByOneCases) {
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

TEST_F(PipPositionerTest, AvoidObstaclesNestedObstacle) {
  const gfx::Rect area(0, 0, 400, 400);
  EXPECT_EQ(gfx::Rect(9, 16, 1, 1),
            CallAvoidObstaclesInternal(
                area, {gfx::Rect(15, 15, 5, 5), gfx::Rect(10, 10, 15, 15)},
                gfx::Rect(16, 16, 1, 1)));
}

TEST_F(PipPositionerTest, AvoidObstaclesAvoidsTwoObstacles) {
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

TEST_F(PipPositionerTest, AvoidObstaclesAvoidsThreeObstacles) {
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

TEST_F(PipPositionerTest, AvoidObstaclesDoesNotPositionBoundsOutsideOfPipArea) {
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

TEST_F(PipPositionerTest, AvoidObstaclesPositionsBoundsWithLeastDisplacement) {
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

TEST_F(PipPositionerTest, AvoidObstaclesAvoidsUnifiedSystemTray) {
  UpdateDisplay("1000x1000");
  auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
  unified_system_tray->ShowBubble(/*show_by_click=*/false);

  auto display = window_state()->GetDisplay();
  gfx::Rect area = PipPositioner::GetMovementArea(display);
  gfx::Rect bubble_bounds = unified_system_tray->GetBubbleBoundsInScreen();
  gfx::Rect bounds = gfx::Rect(bubble_bounds.x(), bubble_bounds.y(), 100, 100);
  gfx::Rect moved_bounds = CallAvoidObstacles(display, bounds);

  // Expect that the returned bounds don't intersect the unified system tray
  // but also don't leave the PIP movement area.
  EXPECT_FALSE(moved_bounds.Intersects(bubble_bounds));
  EXPECT_TRUE(area.Contains(moved_bounds));
}

TEST_F(PipPositionerTest, AvoidObstaclesAvoidsFloatingKeyboard) {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->SetContainerType(
      keyboard::mojom::ContainerType::kFloating, base::nullopt,
      base::DoNothing());
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(200, 200, 100, 100));
  ASSERT_TRUE(keyboard::WaitUntilShown());

  auto display = window_state()->GetDisplay();
  gfx::Rect area = PipPositioner::GetMovementArea(display);
  gfx::Rect moved_bounds =
      CallAvoidObstacles(display, gfx::Rect(150, 200, 100, 100));

  // Expect that the returned bounds don't intersect the floating keyboard
  // but also don't leave the PIP movement area.
  EXPECT_FALSE(moved_bounds.Intersects(keyboard_window->GetBoundsInScreen()));
  EXPECT_TRUE(area.Contains(moved_bounds));
}

TEST_F(PipPositionerTest,
       AvoidObstaclesDoesNotChangeBoundsIfThereIsNoCollision) {
  auto display = window_state()->GetDisplay();
  EXPECT_EQ(gfx::Rect(100, 100, 100, 100),
            CallAvoidObstacles(display, gfx::Rect(100, 100, 100, 100)));
}

}  // namespace ash
