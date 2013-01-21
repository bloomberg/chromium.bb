// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include <algorithm>

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

using aura::test::CreateTestWindowWithId;
using aura::test::TestWindowDelegate;
using aura::Window;

typedef test::AshTestBase WindowCycleControllerTest;

TEST_F(WindowCycleControllerTest, HandleCycleWindowBaseCases) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Cycling doesn't crash if there are no windows.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycling works for a single window, even though nothing changes.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

// Verifies if there is only one window and it isn't active that cycling
// activates it.
TEST_F(WindowCycleControllerTest, SingleWindowNotActive) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Rotate focus, this should move focus to another window that isn't part of
  // the default container.
  Shell::GetInstance()->RotateFocus(Shell::FORWARD);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  // Cycling should activate the window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, HandleCycleWindow) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.  Create them in reverse
  // order so they are stacked 0 over 1 over 2.
  scoped_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  scoped_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->windows());
  ASSERT_EQ(3u, controller->windows()->windows().size());
  ASSERT_EQ(window0.get(), controller->windows()->windows()[0]);
  ASSERT_EQ(window1.get(), controller->windows()->windows()[1]);
  ASSERT_EQ(window2.get(), controller->windows()->windows()[2]);

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
      ash::Shell::GetContainer(
          Shell::GetPrimaryRootWindow(),
          internal::kShellWindowId_SystemModalContainer);
  scoped_ptr<Window> modal_window(
      CreateTestWindowWithId(-2, modal_container));
  modal_window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
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

// Cycles between a maximized and normal window.
TEST_F(WindowCycleControllerTest, MaximizedWindow) {
  // Create a couple of test windows.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  scoped_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  wm::MaximizeWindow(window1.get());
  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Rotate focus, this should move focus to window0.
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // One more time.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
}

// Cycles to a minimized window.
TEST_F(WindowCycleControllerTest, Minimized) {
  // Create a couple of test windows.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  scoped_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  wm::MinimizeWindow(window1.get());
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Rotate focus, this should move focus to window1 and unminimize it.
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_FALSE(wm::IsWindowMinimized(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // One more time back to w0.
  controller->HandleCycleWindow(WindowCycleController::FORWARD, false);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopWindow) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  scoped_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container =
      Shell::GetContainer(
          Shell::GetPrimaryRootWindow(),
          internal::kShellWindowId_AlwaysOnTopContainer);
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->windows());
  ASSERT_EQ(3u, controller->windows()->windows().size());
  EXPECT_EQ(window0.get(), controller->windows()->windows()[0]);
  EXPECT_EQ(window2.get(), controller->windows()->windows()[1]);
  EXPECT_EQ(window1.get(), controller->windows()->windows()[2]);

  controller->AltKeyReleased();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->AltKeyReleased();

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultiWindow) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  scoped_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container =
      Shell::GetContainer(
          Shell::GetPrimaryRootWindow(),
          internal::kShellWindowId_AlwaysOnTopContainer);
  scoped_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  scoped_ptr<Window> window3(CreateTestWindowWithId(3, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->windows());
  ASSERT_EQ(4u, controller->windows()->windows().size());
  EXPECT_EQ(window0.get(), controller->windows()->windows()[0]);
  EXPECT_EQ(window3.get(), controller->windows()->windows()[1]);
  EXPECT_EQ(window2.get(), controller->windows()->windows()[2]);
  EXPECT_EQ(window1.get(), controller->windows()->windows()[3]);

  controller->AltKeyReleased();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->AltKeyReleased();

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_AlwaysOnTopMultipleRootWindows \
        DISABLED_AlwaysOnTopMultipleRootWindows
#else
#define MAYBE_AlwaysOnTopMultipleRootWindows \
        AlwaysOnTopMultipleRootWindows
#endif


TEST_F(WindowCycleControllerTest, MAYBE_AlwaysOnTopMultipleRootWindows) {
  // Set up a second root window
  UpdateDisplay("0+0-1000x600,1001+0-600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  Shell::GetInstance()->set_active_root_window(root_windows[0]);

  // Create two windows in the primary root.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  Window* top_container0 =
      Shell::GetContainer(
          root_windows[0],
          internal::kShellWindowId_AlwaysOnTopContainer);
  scoped_ptr<Window> window1(CreateTestWindowWithId(1, top_container0));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());

  // And two on the secondary root.
  Shell::GetInstance()->set_active_root_window(root_windows[1]);
  scoped_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  Window* top_container1 =
      Shell::GetContainer(
          root_windows[1],
          internal::kShellWindowId_AlwaysOnTopContainer);
  scoped_ptr<Window> window3(CreateTestWindowWithId(3, top_container1));
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  // Move the active root window to the secondary.
  Shell::GetInstance()->set_active_root_window(root_windows[1]);

  wm::ActivateWindow(window2.get());

  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->windows());
  ASSERT_EQ(4u, controller->windows()->windows().size());
  EXPECT_EQ(window2.get(), controller->windows()->windows()[0]);
  EXPECT_EQ(window3.get(), controller->windows()->windows()[1]);
  EXPECT_EQ(window1.get(), controller->windows()->windows()[2]);
  EXPECT_EQ(window0.get(), controller->windows()->windows()[3]);

  controller->AltKeyReleased();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->AltKeyReleased();

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

TEST_F(WindowCycleControllerTest, MostRecentlyUsed) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  scoped_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  scoped_ptr<Window> window2(CreateTestWindowInShellWithId(2));

  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->windows());
  ASSERT_EQ(3u, controller->windows()->windows().size());
  EXPECT_EQ(window0.get(), controller->windows()->windows()[0]);
  EXPECT_EQ(window2.get(), controller->windows()->windows()[1]);
  EXPECT_EQ(window1.get(), controller->windows()->windows()[2]);

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  controller->AltKeyReleased();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));


  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->AltKeyReleased();

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD, true);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

}  // namespace

}  // namespace ash
