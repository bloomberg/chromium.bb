// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <memory>
#include <string>

#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

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
    keyboard::SetTouchKeyboardEnabled(true);
    Shell::Get()->EnableKeyboard();

    UpdateWorkArea("400x400");
    window_ = CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100));
    wm::WindowState* window_state = wm::GetWindowState(window_);
    test_state_ = new FakeWindowState(mojom::WindowStateType::PIP);
    window_state->SetStateObject(
        std::unique_ptr<wm::WindowState::State>(test_state_));
  }

  void TearDown() override {
    keyboard::SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    aura::Window* root = Shell::GetPrimaryRootWindow();
    Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
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
  EXPECT_EQ("8,8 384x384", area.ToString());
}

TEST_F(PipPositionerTest, PipMovementAreaIncludesKeyboardIfKeyboardIsShown) {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(true /* lock */);
  keyboard_controller->NotifyKeyboardWindowLoaded();

  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(0, 300, 400, 100));

  gfx::Rect area = PipPositioner::GetMovementArea(window_state()->GetDisplay());
  EXPECT_EQ(gfx::Rect(8, 8, 384, 284 - ShelfConstants::shelf_size()), area);
}

TEST_F(PipPositionerTest, PipRestingPositionSnapsToClosestEdge) {
  auto display = window_state()->GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ("100,8 100x100", PipPositioner::GetRestingPosition(
                                 display, gfx::Rect(100, 50, 100, 100))
                                 .ToString());

  // Snap near bottom edge to bottom.
  EXPECT_EQ("100,292 100x100", PipPositioner::GetRestingPosition(
                                   display, gfx::Rect(100, 250, 100, 100))
                                   .ToString());

  // Snap near left edge to left.
  EXPECT_EQ("8,100 100x100", PipPositioner::GetRestingPosition(
                                 display, gfx::Rect(50, 100, 100, 100))
                                 .ToString());

  // Snap near right edge to right.
  EXPECT_EQ("292,100 100x100", PipPositioner::GetRestingPosition(
                                   display, gfx::Rect(250, 100, 100, 100))
                                   .ToString());
}

TEST_F(PipPositionerTest, PipRestingPositionSnapsInsideDisplay) {
  auto display = window_state()->GetDisplay();

  // Snap near top edge outside movement area to top.
  EXPECT_EQ("100,8 100x100", PipPositioner::GetRestingPosition(
                                 display, gfx::Rect(100, -50, 100, 100))
                                 .ToString());

  // Snap near bottom edge outside movement area to bottom.
  EXPECT_EQ("100,292 100x100", PipPositioner::GetRestingPosition(
                                   display, gfx::Rect(100, 450, 100, 100))
                                   .ToString());

  // Snap near left edge outside movement area to left.
  EXPECT_EQ("8,100 100x100", PipPositioner::GetRestingPosition(
                                 display, gfx::Rect(-50, 100, 100, 100))
                                 .ToString());

  // Snap near right edge outside movement area to right.
  EXPECT_EQ("292,100 100x100", PipPositioner::GetRestingPosition(
                                   display, gfx::Rect(450, 100, 100, 100))
                                   .ToString());
}

TEST_F(PipPositionerTest, PipAdjustPositionForDragClampsToMovementArea) {
  auto display = window_state()->GetDisplay();

  // Adjust near top edge outside movement area.
  EXPECT_EQ("100,8 100x100", PipPositioner::GetBoundsForDrag(
                                 display, gfx::Rect(100, -50, 100, 100))
                                 .ToString());

  // Adjust near bottom edge outside movement area.
  EXPECT_EQ("100,292 100x100", PipPositioner::GetBoundsForDrag(
                                   display, gfx::Rect(100, 450, 100, 100))
                                   .ToString());

  // Adjust near left edge outside movement area.
  EXPECT_EQ("8,100 100x100", PipPositioner::GetBoundsForDrag(
                                 display, gfx::Rect(-50, 100, 100, 100))
                                 .ToString());

  // Adjust near right edge outside movement area.
  EXPECT_EQ("292,100 100x100", PipPositioner::GetBoundsForDrag(
                                   display, gfx::Rect(450, 100, 100, 100))
                                   .ToString());
}

TEST_F(PipPositionerTest, PipRestingPositionWorksIfKeyboardIsDisabled) {
  Shell::Get()->DisableKeyboard();
  auto display = window_state()->GetDisplay();

  // Snap near top edge to top.
  EXPECT_EQ("100,8 100x100", PipPositioner::GetRestingPosition(
                                 display, gfx::Rect(100, 50, 100, 100))
                                 .ToString());
}

}  // namespace ash
