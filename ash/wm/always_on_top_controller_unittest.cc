// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace ash {

class VirtualKeyboardAlwaysOnTopControllerTest : public AshTestBase {
 public:
  VirtualKeyboardAlwaysOnTopControllerTest() = default;
  ~VirtualKeyboardAlwaysOnTopControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardAlwaysOnTopControllerTest);
};

class TestLayoutManager : public WorkspaceLayoutManager {
 public:
  explicit TestLayoutManager(aura::Window* window)
      : WorkspaceLayoutManager(window), keyboard_bounds_changed_(false) {}

  ~TestLayoutManager() override = default;

  void OnKeyboardWorkspaceDisplacingBoundsChanged(
      const gfx::Rect& bounds) override {
    keyboard_bounds_changed_ = true;
    WorkspaceLayoutManager::OnKeyboardWorkspaceDisplacingBoundsChanged(bounds);
  }

  bool keyboard_bounds_changed() const { return keyboard_bounds_changed_; }

 private:
  bool keyboard_bounds_changed_;
  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// Verifies that the always on top controller is notified of keyboard bounds
// changing events.
TEST_F(VirtualKeyboardAlwaysOnTopControllerTest, NotifyKeyboardBoundsChanging) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* always_on_top_container =
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer);
  // Install test layout manager.
  TestLayoutManager* manager = new TestLayoutManager(always_on_top_container);
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();
  always_on_top_controller->SetLayoutManagerForTest(base::WrapUnique(manager));

  // Show the keyboard.
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Verify that test manager was notified of bounds change.
  ASSERT_TRUE(manager->keyboard_bounds_changed());
}

}  // namespace ash
