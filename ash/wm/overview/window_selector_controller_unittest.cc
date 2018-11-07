// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_resizer.h"
#include "base/command_line.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace {

gfx::Point CalculateDragPoint(const ash::WindowResizer& resizer,
                              int delta_x,
                              int delta_y) {
  gfx::Point location = resizer.GetInitialLocation();
  location.set_x(location.x() + delta_x);
  location.set_y(location.y() + delta_y);
  return location;
}

}  // namespace

namespace ash {

using WindowSelectorControllerTest = AshTestBase;

// Tests that press the overview key in keyboard when a window is being dragged
// in clamshell mode should not toggle overview.
TEST_F(WindowSelectorControllerTest,
       PressOverviewKeyDuringWindowDragInClamshellMode) {
  ASSERT_FALSE(TabletModeControllerTestApi().IsTabletModeStarted());
  std::unique_ptr<aura::Window> dragged_window = CreateTestWindow();
  std::unique_ptr<WindowResizer> resizer =
      CreateWindowResizer(dragged_window.get(), gfx::Point(), HTCAPTION,
                          ::wm::WINDOW_MOVE_SOURCE_MOUSE);
  resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
  EXPECT_TRUE(wm::GetWindowState(dragged_window.get())->is_dragged());
  GetEventGenerator()->PressKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  resizer->CompleteDrag();
}

class OverviewVirtualKeyboardTest : public WindowSelectorControllerTest {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    WindowSelectorControllerTest::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());

    keyboard_controller()->LoadKeyboardWindowInBackground();
    keyboard_controller()->GetKeyboardWindow()->SetBounds(
        keyboard::KeyboardBoundsFromRootBounds(
            Shell::GetPrimaryRootWindow()->bounds(), 100));
    // Wait for keyboard window to load.
    base::RunLoop().RunUntilIdle();
  }

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard::KeyboardController::Get();
  }
};

TEST_F(OverviewVirtualKeyboardTest, ToggleOverviewModeHidesVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(false /* locked */);
  keyboard::WaitUntilShown();

  Shell::Get()->window_selector_controller()->ToggleOverview();

  // Timeout failure here if the keyboard does not hide.
  keyboard::WaitUntilHidden();
}

TEST_F(OverviewVirtualKeyboardTest,
       ToggleOverviewModeDoesNotHideLockedVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(true /* locked */);
  keyboard::WaitUntilShown();

  Shell::Get()->window_selector_controller()->ToggleOverview();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());
}

}  // namespace ash
