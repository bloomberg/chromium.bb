// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/workspace/workspace_layout_manager_delegate.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace test {

class VirtualKeyboardAlwaysOnTopControllerTest : public AshTestBase {
 public:
  VirtualKeyboardAlwaysOnTopControllerTest() {}
  ~VirtualKeyboardAlwaysOnTopControllerTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    test::AshTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardAlwaysOnTopControllerTest);
};

class TestLayoutManager : public WorkspaceLayoutManager {
 public:
  explicit TestLayoutManager(wm::WmWindow* window)
      : WorkspaceLayoutManager(window, nullptr),
        keyboard_bounds_changed_(false) {}

  ~TestLayoutManager() override {}

  void OnKeyboardBoundsChanging(const gfx::Rect& bounds) override {
    keyboard_bounds_changed_ = true;
    WorkspaceLayoutManager::OnKeyboardBoundsChanging(bounds);
  }

  bool keyboard_bounds_changed() const { return keyboard_bounds_changed_; }

 private:
  bool keyboard_bounds_changed_;
  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// Verifies that the always on top controller is notified of keyboard bounds
// changing events.
TEST_F(VirtualKeyboardAlwaysOnTopControllerTest, NotifyKeyboardBoundsChanged) {
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* always_on_top_container =
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer);
  // Install test layout manager.
  TestLayoutManager* manager =
      new TestLayoutManager(wm::WmWindowAura::Get(always_on_top_container));
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();
  always_on_top_controller->SetLayoutManagerForTest(base::WrapUnique(manager));
  // Activate keyboard. This triggers keyboard listeners to be registered.
  controller->ActivateKeyboard(keyboard_controller);

  // Mock a keyboard appearing.
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  ASSERT_TRUE(keyboard_container);
  keyboard_container->Show();
  aura::Window* keyboard_window =
      keyboard_controller->ui()->GetKeyboardWindow();
  keyboard_container->AddChild(keyboard_window);
  keyboard_window->set_owned_by_parent(false);
  const int kKeyboardHeight = 200;
  gfx::Rect keyboard_bounds = keyboard::FullWidthKeyboardBoundsFromRootBounds(
      root_window->bounds(), kKeyboardHeight);
  keyboard_window->SetBounds(keyboard_bounds);
  keyboard_window->Show();
  keyboard_controller->NotifyKeyboardBoundsChanging(keyboard_bounds);
  // Verify that test manager was notified of bounds change.
  ASSERT_TRUE(manager->keyboard_bounds_changed());
}

}  // namespace test
}  // namespace ash
