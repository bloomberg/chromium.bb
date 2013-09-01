// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace internal {

class WindowSelectorTest : public test::AshTestBase {
 public:
  WindowSelectorTest() {}
  virtual ~WindowSelectorTest() {}

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithDelegate(&wd, -1, bounds);
  }

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) {
    gfx::RectF window1_bounds = GetTransformedTargetBounds(window1);
    gfx::RectF window2_bounds = GetTransformedTargetBounds(window2);
    return window1_bounds.Intersects(window2_bounds);
  }

  void ToggleOverview() {
    ash::Shell::GetInstance()->window_selector_controller()->ToggleOverview();
  }

  void Cycle(WindowSelector::Direction direction) {
    ash::Shell::GetInstance()->window_selector_controller()->
        HandleCycleWindow(direction);
  }

  void StopCycling() {
    ash::Shell::GetInstance()->window_selector_controller()->AltKeyReleased();
  }

  void FireOverviewStartTimer() {
    // Calls the method to start overview mode which is normally called by the
    // timer. The timer will still fire and call this method triggering the
    // DCHECK that overview mode was not already started, except that we call
    // StopCycling before the timer has a chance to fire.
    ash::Shell::GetInstance()->window_selector_controller()->window_selector_->
        StartOverview();
  }

  gfx::RectF GetTransformedBounds(aura::Window* window) {
    gfx::RectF bounds(window->layer()->bounds());
    window->layer()->transform().TransformRect(&bounds);
    return bounds;
  }

  gfx::RectF GetTransformedTargetBounds(aura::Window* window) {
    gfx::RectF bounds(window->layer()->GetTargetBounds());
    window->layer()->GetTargetTransform().TransformRect(&bounds);
    return bounds;
  }

  void ClickWindow(aura::Window* window) {
    aura::test::EventGenerator event_generator(window->GetRootWindow(), window);
    gfx::RectF target = GetTransformedBounds(window);
    event_generator.ClickLeftButton();
  }

  bool IsSelecting() {
    return ash::Shell::GetInstance()->window_selector_controller()->
        IsSelecting();
  }

  aura::Window* GetFocusedWindow() {
    return aura::client::GetFocusClient(
        Shell::GetActiveRootWindow())->GetFocusedWindow();
  }

 private:
  aura::test::TestWindowDelegate wd;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorTest);
};

// Tests entering overview mode with two windows and selecting one.
TEST_F(WindowSelectorTest, Basic) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window2.get(), GetFocusedWindow());

  // In overview mode the windows should no longer overlap and focus should
  // be removed from the window.
  ToggleOverview();
  EXPECT_EQ(NULL, GetFocusedWindow());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window1.get(), GetFocusedWindow());
}

// Tests entering overview mode with three windows and cycling through them.
TEST_F(WindowSelectorTest, BasicCycle) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window3.get()));

  Cycle(WindowSelector::FORWARD);
  EXPECT_TRUE(IsSelecting());
  Cycle(WindowSelector::FORWARD);
  StopCycling();
  EXPECT_FALSE(IsSelecting());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));
}

// Tests that a newly created window aborts overview.
TEST_F(WindowSelectorTest, NewWindowCancelsOveriew) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  // A window being created should exit overview mode.
  scoped_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_FALSE(IsSelecting());
}

// Tests that a window activation exits overview mode.
TEST_F(WindowSelectorTest, ActivationCancelsOveriew) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  window2->Focus();
  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  // A window being activated should exit overview mode.
  window1->Focus();
  EXPECT_FALSE(IsSelecting());

  // window1 should be focused after exiting even though window2 was focused on
  // entering overview because we exited due to an activation.
  EXPECT_EQ(window1.get(), GetFocusedWindow());
}

// Verifies that overview mode only begins after a delay when cycling.
TEST_F(WindowSelectorTest, CycleOverviewDelay) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));

  // When cycling first starts, the windows will still be overlapping.
  Cycle(WindowSelector::FORWARD);
  EXPECT_TRUE(IsSelecting());
  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));

  // Once the overview timer fires, the windows should no longer overlap.
  FireOverviewStartTimer();
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  StopCycling();
}

// Tests that exiting overview mode without selecting a window restores focus
// to the previously focused window.
TEST_F(WindowSelectorTest, CancelRestoresFocus) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window(CreateWindow(bounds));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), GetFocusedWindow());

  // In overview mode, focus should be removed.
  ToggleOverview();
  EXPECT_EQ(NULL, GetFocusedWindow());

  // If canceling overview mode, focus should be restored.
  ToggleOverview();
  EXPECT_EQ(window.get(), GetFocusedWindow());
}

// Tests that overview mode is exited if the last remaining window is destroyed.
TEST_F(WindowSelectorTest, LastWindowDestroyed) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();

  window1.reset();
  window2.reset();
  EXPECT_FALSE(IsSelecting());
}

// Tests that entering overview mode restores a window to its original
// target location.
TEST_F(WindowSelectorTest, QuickReentryRestoresInitialTransform) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window(CreateWindow(bounds));
  gfx::Rect initial_bounds = ToEnclosingRect(
      GetTransformedBounds(window.get()));
  ToggleOverview();
  // Quickly exit and reenter overview mode. The window should still be
  // animating when we reenter. We cannot short circuit animations for this but
  // we also don't have to wait for them to complete.
  {
    ui::ScopedAnimationDurationScaleMode normal_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
    ToggleOverview();
    ToggleOverview();
  }
  EXPECT_NE(initial_bounds, ToEnclosingRect(
      GetTransformedTargetBounds(window.get())));
  ToggleOverview();
  EXPECT_FALSE(IsSelecting());
  EXPECT_EQ(initial_bounds, ToEnclosingRect(
      GetTransformedTargetBounds(window.get())));
}

// Tests that windows remain on the display they are currently on in overview
// mode.
TEST_F(WindowSelectorTest, MultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> window1(CreateWindow(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> window2(CreateWindow(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> window3(CreateWindow(gfx::Rect(450, 0, 100, 100)));
  scoped_ptr<aura::Window> window4(CreateWindow(gfx::Rect(450, 0, 100, 100)));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());

  // In overview mode, each window remains in the same root window.
  ToggleOverview();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());
  root_windows[0]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window1.get())));
  root_windows[0]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window2.get())));
  root_windows[1]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window3.get())));
  root_windows[1]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window4.get())));
}

}  // namespace internal
}  // namespace ash
