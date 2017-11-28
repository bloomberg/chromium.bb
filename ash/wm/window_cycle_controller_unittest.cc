// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/test_app_list_presenter_impl.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

class EventCounter : public ui::EventHandler {
 public:
  EventCounter() : key_events_(0), mouse_events_(0) {}
  ~EventCounter() override = default;

  int GetKeyEventCountAndReset() {
    int count = key_events_;
    key_events_ = 0;
    return count;
  }

  int GetMouseEventCountAndReset() {
    int count = mouse_events_;
    mouse_events_ = 0;
    return count;
  }

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override { key_events_++; }
  void OnMouseEvent(ui::MouseEvent* event) override { mouse_events_++; }

 private:
  int key_events_;
  int mouse_events_;

  DISALLOW_COPY_AND_ASSIGN(EventCounter);
};

bool IsWindowMinimized(aura::Window* window) {
  return wm::GetWindowState(window)->IsMinimized();
}

}  // namespace

using aura::test::CreateTestWindowWithId;
using aura::test::TestWindowDelegate;
using aura::Window;

class WindowCycleControllerTest : public AshTestBase {
 public:
  WindowCycleControllerTest() = default;
  ~WindowCycleControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    WindowCycleList::DisableInitialDelayForTesting();

    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(1);
  }

  aura::Window* CreatePanelWindow() {
    gfx::Rect rect(100, 100);
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL, aura::client::WINDOW_TYPE_PANEL, 0, rect);
    shelf_view_test_->RunMessageLoopUntilAnimationsDone();
    return window;
  }

  const aura::Window::Windows GetWindows(WindowCycleController* controller) {
    return controller->window_cycle_list()->windows();
  }

  const views::Widget* GetWindowCycleListWidget() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->widget();
  }

 private:
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleControllerTest);
};

TEST_F(WindowCycleControllerTest, HandleCycleWindowBaseCases) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

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
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Rotate focus, this should move focus to another window that isn't part of
  // the default container.
  Shell::Get()->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  // Cycling should activate the window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, HandleCycleWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

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

  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Pressing and releasing Alt-tab again should cycle back to the most-
  // recently-used window in the current child order.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cancelled cycling shouldn't move the active window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CancelCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Pressing Alt-tab multiple times without releasing Alt should cycle through
  // all the windows and wrap around.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());

  controller->CompleteCycling();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // Likewise we can cycle backwards through the windows.
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // When the screen is locked, cycling window does not take effect.
  Shell::Get()->session_controller()->LockScreenAndFlushForTest();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(controller->IsCycling());

  // Unlock, it works again.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // When a modal window is active, cycling window does not take effect.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SystemModalContainer);
  std::unique_ptr<Window> modal_window(
      CreateTestWindowWithId(-2, modal_container));
  modal_window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  wm::ActivateWindow(modal_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));

  modal_window.reset();
  std::unique_ptr<Window> skip_overview_window(
      CreateTestWindowInShellWithId(-3));
  skip_overview_window->SetProperty(kShowInOverviewKey, false);
  wm::ActivateWindow(window0.get());
  wm::ActivateWindow(skip_overview_window.get());
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(skip_overview_window.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
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
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::GetWindowState(window0.get())->IsActive());
  EXPECT_FALSE(window1_state->IsActive());

  // One more time.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
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
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_FALSE(window0_state->IsActive());
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsActive());

  // One more time back to w0.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(window0_state->IsActive());
}

// Tests that when all windows are minimized, cycling starts with the first one
// rather than the second.
TEST_F(WindowCycleControllerTest, AllAreMinimized) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  wm::WindowState* window0_state = wm::GetWindowState(window0.get());
  wm::WindowState* window1_state = wm::GetWindowState(window1.get());

  window0_state->Minimize();
  window1_state->Minimize();

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(window0_state->IsActive());
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // But it's business as usual when cycling backwards.
  window0_state->Minimize();
  window1_state->Minimize();
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsActive());
  EXPECT_FALSE(window1_state->IsMinimized());
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
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

  controller->CompleteCycling();
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultiWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
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

  controller->CompleteCycling();
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultipleRootWindows) {
  // Set up a second root window
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Create two windows in the primary root.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  Window* top_container0 =
      Shell::GetContainer(root_windows[0], kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window1(CreateTestWindowWithId(1, top_container0));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());

  // Move the active root window to the secondary root and create two windows.
  ScopedRootWindowForNewWindows root_for_new_windows(root_windows[1]);
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  Window* top_container1 =
      Shell::GetContainer(root_windows[1], kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window3(CreateTestWindowWithId(3, top_container1));
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

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

  controller->CompleteCycling();
}

TEST_F(WindowCycleControllerTest, MostRecentlyUsed) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

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

  // Cycling through then stopping the cycling will activate a window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Cycling alone (without CompleteCycling()) doesn't activate.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  // Showing the Alt+Tab UI does however deactivate the erstwhile active window.
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));

  controller->CompleteCycling();
}

// Tests that beginning window selection hides the app list.
TEST_F(WindowCycleControllerTest, SelectingHidesAppList) {
  // The tested behavior relies on the app list presenter implementation.
  TestAppListPresenterImpl app_list_presenter_impl;

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  app_list_presenter_impl.ShowAndRunLoop(GetPrimaryDisplay().id());
  EXPECT_TRUE(app_list_presenter_impl.IsVisible());
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(app_list_presenter_impl.IsVisible());

  // Make sure that dismissing the app list this way doesn't pass activation
  // to a different window.
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));

  controller->CompleteCycling();
}

// Tests that cycling through windows doesn't change their minimized state.
TEST_F(WindowCycleControllerTest, CyclePreservesMinimization) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window1.get());
  wm::GetWindowState(window1.get())->Minimize();
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  // On window 2.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  // Back on window 1.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  controller->CompleteCycling();

  EXPECT_TRUE(IsWindowMinimized(window1.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowCycleControllerTest, CyclePanels) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> panel0(CreatePanelWindow());
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow());
  wm::ActivateWindow(window0.get());
  wm::ActivateWindow(panel1.get());
  wm::ActivateWindow(panel0.get());
  EXPECT_TRUE(wm::IsActiveWindow(panel0.get()));

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel1.get()));

  // Cycling again should select the most recently used panel.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel0.get()));

  // Cycling twice again should select the first window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowCycleControllerTest, CyclePanelsDestroyed) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

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
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowCycleControllerTest, CycleMruPanelDestroyed) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

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
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel1.get()));
}

// Tests that the tab key events are not sent to the window.
TEST_F(WindowCycleControllerTest, TabKeyNotLeaked) {
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->AddPreTargetHandler(&event_count);
  ui::test::EventGenerator& generator = GetEventGenerator();
  wm::GetWindowState(w0.get())->Activate();
  generator.PressKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_EQ(1, event_count.GetKeyEventCountAndReset());
  generator.PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
  generator.ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
  generator.ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_TRUE(wm::GetWindowState(w1.get())->IsActive());
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
}

// While the UI is active, mouse events are captured.
TEST_F(WindowCycleControllerTest, MouseEventsCaptured) {
  // This delegate allows the window to receive mouse events.
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->SetTargetHandler(&event_count);
  ui::test::EventGenerator& generator = GetEventGenerator();
  wm::ActivateWindow(w0.get());

  // Events get through.
  generator.MoveMouseToCenterOf(w0.get());
  generator.ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Start cycling.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Most mouse events don't get through.
  generator.PressLeftButton();
  EXPECT_EQ(0, event_count.GetMouseEventCountAndReset());

  // Although releases do.
  generator.ReleaseLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Stop cycling: once again, events get through.
  controller->CompleteCycling();
  generator.ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());
}

// Tests that we can cycle past fullscreen windows: https://crbug.com/622396.
// Fullscreen windows are special in that they are allowed to handle alt+tab
// keypresses, which means the window cycle event filter should not handle
// the tab press else it prevents cycling past that window.
TEST_F(WindowCycleControllerTest, TabPastFullscreenWindow) {
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  wm::WMEvent maximize_event(wm::WM_EVENT_FULLSCREEN);

  // To make this test work with or without the new alt+tab selector we make
  // both the initial window and the second window fullscreen.
  wm::GetWindowState(w0.get())->OnWMEvent(&maximize_event);
  wm::GetWindowState(w1.get())->Activate();
  wm::GetWindowState(w1.get())->OnWMEvent(&maximize_event);
  EXPECT_TRUE(wm::GetWindowState(w0.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(w1.get())->IsFullscreen());
  wm::GetWindowState(w0.get())->Activate();
  EXPECT_TRUE(wm::GetWindowState(w0.get())->IsActive());

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::VKEY_MENU, ui::EF_NONE);

  generator.PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  generator.ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);

  // Because w0 and w1 are full-screen, the event should be passed to the
  // browser window to handle it (which if the browser doesn't handle it will
  // pass on the alt+tab to continue cycling). To make this test work with or
  // without the new alt+tab selector we check for the event on either
  // fullscreen window.
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->AddPreTargetHandler(&event_count);
  generator.PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(1, event_count.GetKeyEventCountAndReset());
}

// Tests that the Alt+Tab UI's position isn't affected by the origin of the
// display it's on. See crbug.com/675718
TEST_F(WindowCycleControllerTest, MultiDisplayPositioning) {
  int64_t primary_id = GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(2, primary_id, primary_id + 1);

  auto placements = {
      display::DisplayPlacement::BOTTOM, display::DisplayPlacement::TOP,
      display::DisplayPlacement::LEFT, display::DisplayPlacement::RIGHT,
  };

  gfx::Rect expected_bounds;
  for (auto placement : placements) {
    SCOPED_TRACE(placement);

    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id, placement, 0);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    // Use two displays.
    UpdateDisplay("500x500,600x600");

    gfx::Rect second_display_bounds =
        display_manager()->GetDisplayAt(1).bounds();
    std::unique_ptr<Window> window0(
        CreateTestWindowInShellWithBounds(second_display_bounds));
    // Activate this window so that the secondary display becomes the one where
    // the Alt+Tab UI is shown.
    wm::ActivateWindow(window0.get());
    std::unique_ptr<Window> window1(
        CreateTestWindowInShellWithBounds(second_display_bounds));

    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->HandleCycleWindow(WindowCycleController::FORWARD);

    const gfx::Rect bounds =
        GetWindowCycleListWidget()->GetWindowBoundsInScreen();
    EXPECT_TRUE(second_display_bounds.Contains(bounds));
    EXPECT_FALSE(
        display_manager()->GetDisplayAt(0).bounds().Intersects(bounds));
    const gfx::Rect display_relative_bounds =
        bounds - second_display_bounds.OffsetFromOrigin();
    // Base case sets the expectation for other cases.
    if (expected_bounds.IsEmpty())
      expected_bounds = display_relative_bounds;
    else
      EXPECT_EQ(expected_bounds, display_relative_bounds);
    controller->CompleteCycling();
  }
}

}  // namespace ash
