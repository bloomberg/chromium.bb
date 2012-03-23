// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"

namespace ash {

namespace {

using aura::test::CreateTestWindowWithId;
using aura::test::TestWindowDelegate;
using aura::Window;

class WindowCycleControllerTest : public test::AshTestBase {
 public:
  WindowCycleControllerTest() {}
  virtual ~WindowCycleControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowCycleControllerTest);
};

TEST_F(WindowCycleControllerTest, HandleCycleWindowBaseCases) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Cycling doesn't crash if there are no windows.
  std::vector<Window*> windows = Shell::GetInstance()->delegate()->
      GetCycleWindowList(ShellDelegate::SOURCE_KEYBOARD);
  EXPECT_TRUE(windows.empty());
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycling works for a single window, even though nothing changes.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, HandleCycleWindow) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.  Create them in reverse
  // order so they are stacked 0 over 1 over 2.
  Window* default_container =
      Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, default_container));
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, default_container));
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  wm::ActivateWindow(window0.get());

  // Window lists should return the topmost window in front.
  std::vector<Window*> windows = Shell::GetInstance()->delegate()->
      GetCycleWindowList(ShellDelegate::SOURCE_KEYBOARD);
  ASSERT_EQ(3u, windows.size());
  ASSERT_EQ(window0.get(), windows[0]);
  ASSERT_EQ(window1.get(), windows[1]);
  ASSERT_EQ(window2.get(), windows[2]);

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  controller->AltKeyReleased();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Pressing and releasing Alt-tab again should cycle back to the most-
  // recently-used window in the current child order.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  controller->AltKeyReleased();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Pressing Alt-tab multiple times without releasing Alt should cycle through
  // all the windows and wrap around.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->AltKeyReleased();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // Likewise we can cycle backwards through all the windows.
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->AltKeyReleased();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Passing false for is_alt_down does not start a cycle gesture.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // When the screen is locked, cycling window does not take effect.
  Shell::GetInstance()->delegate()->LockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  Shell::GetInstance()->delegate()->UnlockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // When a modal window is active, cycling window does not take effect.
  aura::Window* modal_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_SystemModalContainer);
  scoped_ptr<Window> modal_window(
      CreateTestWindowWithId(-2, modal_container));
  wm::ActivateWindow(modal_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
}

}  // namespace

}  // namespace ash
