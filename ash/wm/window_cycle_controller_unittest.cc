// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include <algorithm>
#include <memory>

#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/screen.h"

namespace ash {

using aura::test::CreateTestWindowWithId;
using aura::test::TestWindowDelegate;
using aura::Window;

class WindowCycleControllerTest : public test::AshTestBase {
 public:
  WindowCycleControllerTest() {}
  ~WindowCycleControllerTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    ASSERT_TRUE(test::TestShelfDelegate::instance());

    shelf_view_test_.reset(new test::ShelfViewTestAPI(
        test::ShelfTestAPI(Shelf::ForPrimaryDisplay()).shelf_view()));
    shelf_view_test_->SetAnimationDuration(1);
  }

  aura::Window* CreatePanelWindow() {
    gfx::Rect rect(100, 100);
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL, ui::wm::WINDOW_TYPE_PANEL, 0, rect);
    test::TestShelfDelegate::instance()->AddShelfItem(window);
    shelf_view_test_->RunMessageLoopUntilAnimationsDone();
    return window;
  }

  const WindowCycleList::WindowList& GetWindows(
      WindowCycleController* controller) {
    return controller->window_cycle_list()->windows();
  }

 private:
  std::unique_ptr<test::ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleControllerTest);
};

TEST_F(WindowCycleControllerTest, HandleCycleWindowBaseCases) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Cycling doesn't crash if there are no windows.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycling works for a single window, even though nothing changes.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

// Verifies if there is only one window and it isn't active that cycling
// activates it.
TEST_F(WindowCycleControllerTest, SingleWindowNotActive) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Rotate focus, this should move focus to another window that isn't part of
  // the default container.
  Shell::GetInstance()->RotateFocus(Shell::FORWARD);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  // Cycling should activate the window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, HandleCycleWindow) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.  Create them in reverse
  // order so they are stacked 0 over 1 over 2.
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  ASSERT_EQ(window0.get(), GetWindows(controller)[0]);
  ASSERT_EQ(window1.get(), GetWindows(controller)[1]);
  ASSERT_EQ(window2.get(), GetWindows(controller)[2]);

  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Pressing and releasing Alt-tab again should cycle back to the most-
  // recently-used window in the current child order.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Pressing Alt-tab multiple times without releasing Alt should cycle through
  // all the windows and wrap around.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->StopCycling();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // Likewise we can cycle backwards through all the windows.
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // When the screen is locked, cycling window does not take effect.
  Shell::GetInstance()->session_state_delegate()->LockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  Shell::GetInstance()->session_state_delegate()->UnlockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // When a modal window is active, cycling window does not take effect.
  aura::Window* modal_container =
      ash::Shell::GetContainer(
          Shell::GetPrimaryRootWindow(),
          kShellWindowId_SystemModalContainer);
  std::unique_ptr<Window> modal_window(
      CreateTestWindowWithId(-2, modal_container));
  modal_window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  wm::ActivateWindow(modal_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
}

// Cycles between a maximized and normal window.
TEST_F(WindowCycleControllerTest, MaximizedWindow) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  wm::WindowState* window1_state = wm::GetWindowState(window1.get());
  window1_state->Maximize();
  window1_state->Activate();
  EXPECT_TRUE(window1_state->IsActive());

  // Rotate focus, this should move focus to window0.
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::GetWindowState(window0.get())->IsActive());

  // One more time.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(window1_state->IsActive());
}

// Cycles to a minimized window.
TEST_F(WindowCycleControllerTest, Minimized) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  wm::WindowState* window0_state = wm::GetWindowState(window0.get());
  wm::WindowState* window1_state = wm::GetWindowState(window1.get());

  window1_state->Minimize();
  window0_state->Activate();
  EXPECT_TRUE(window0_state->IsActive());

  // Rotate focus, this should move focus to window1 and unminimize it.
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsActive());

  // One more time back to w0.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(window0_state->IsActive());
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopWindow) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container =
      Shell::GetContainer(
          Shell::GetPrimaryRootWindow(),
          kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);

  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->StopCycling();

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultiWindow) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container =
      Shell::GetContainer(
          Shell::GetPrimaryRootWindow(),
          kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  std::unique_ptr<Window> window3(CreateTestWindowWithId(3, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(4u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window3.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[2]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[3]);

  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->StopCycling();

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultipleRootWindows) {
  if (!SupportsMultipleDisplays())
    return;

  // Set up a second root window
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  Shell::GetInstance()->set_target_root_window(root_windows[0]);

  // Create two windows in the primary root.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  Window* top_container0 =
      Shell::GetContainer(
          root_windows[0],
          kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window1(CreateTestWindowWithId(1, top_container0));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());

  // And two on the secondary root.
  Shell::GetInstance()->set_target_root_window(root_windows[1]);
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  Window* top_container1 =
      Shell::GetContainer(
          root_windows[1],
          kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window3(CreateTestWindowWithId(3, top_container1));
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  // Move the active root window to the secondary.
  Shell::GetInstance()->set_target_root_window(root_windows[1]);

  wm::ActivateWindow(window2.get());

  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(4u, GetWindows(controller).size());
  EXPECT_EQ(window2.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window3.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);
  EXPECT_EQ(window0.get(), GetWindows(controller)[3]);

  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->StopCycling();

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

TEST_F(WindowCycleControllerTest, MostRecentlyUsed) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));

  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));


  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  controller->StopCycling();

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

// Tests that beginning window selection hides the app list.
TEST_F(WindowCycleControllerTest, SelectingHidesAppList) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  Shell::GetInstance()->ShowAppList(NULL);
  EXPECT_TRUE(Shell::GetInstance()->GetAppListTargetVisibility());
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(Shell::GetInstance()->GetAppListTargetVisibility());
}

// Tests that cycling through windows shows and minimizes windows as they
// are passed.
TEST_F(WindowCycleControllerTest, CyclePreservesMinimization) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window1.get());
  wm::GetWindowState(window1.get())->Minimize();
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsWindowMinimized(window1.get()));

  // On window 2.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(wm::IsWindowMinimized(window1.get()));

  // Back on window 1.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsWindowMinimized(window1.get()));

  controller->StopCycling();

  EXPECT_TRUE(wm::IsWindowMinimized(window1.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowCycleControllerTest, CyclePanels) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> panel0(CreatePanelWindow());
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow());
  wm::ActivateWindow(window0.get());
  wm::ActivateWindow(panel1.get());
  wm::ActivateWindow(panel0.get());
  EXPECT_TRUE(wm::IsActiveWindow(panel0.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel1.get()));

  // Cycling again should select the most recently used panel.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel0.get()));

  // Cycling twice again should select the first window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowCycleControllerTest, CyclePanelsDestroyed) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  std::unique_ptr<aura::Window> panel0(CreatePanelWindow());
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(panel1.get());
  wm::ActivateWindow(panel0.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycling once highlights window2.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  // All panels are destroyed.
  panel0.reset();
  panel1.reset();
  // Cycling again should now select window2.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowCycleControllerTest, CycleMruPanelDestroyed) {
  WindowCycleController* controller =
      Shell::GetInstance()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> panel0(CreatePanelWindow());
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow());
  wm::ActivateWindow(panel1.get());
  wm::ActivateWindow(panel0.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycling once highlights window2.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Panel 1 is the next item as the MRU panel, removing it should make panel 2
  // the next window to be selected.
  panel0.reset();
  // Cycling again should now select panel1.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel1.get()));
}

}  // namespace ash
