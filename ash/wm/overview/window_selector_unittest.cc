// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_test_api.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
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

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    ASSERT_TRUE(test::TestLauncherDelegate::instance());

    launcher_view_test_.reset(new test::LauncherViewTestAPI(
        test::LauncherTestAPI(Launcher::ForPrimaryDisplay()).launcher_view()));
    launcher_view_test_->SetAnimationDuration(1);
  }

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithDelegate(&wd, -1, bounds);
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL, aura::client::WINDOW_TYPE_PANEL, 0, bounds);
    test::TestLauncherDelegate::instance()->AddLauncherItem(window);
    launcher_view_test()->RunMessageLoopUntilAnimationsDone();
    return window;
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
    ash::Shell::GetInstance()->window_selector_controller()->window_selector_->
        SelectWindow();
  }

  void FireOverviewStartTimer() {
    // Calls the method to start overview mode which is normally called by the
    // timer. The timer will still fire and call this method triggering the
    // DCHECK that overview mode was not already started, except that we call
    // StopCycling before the timer has a chance to fire.
    ash::Shell::GetInstance()->window_selector_controller()->window_selector_->
        StartOverview();
  }

  gfx::Transform GetTransformRelativeTo(gfx::PointF origin,
                                        const gfx::Transform& transform) {
    gfx::Transform t;
    t.Translate(origin.x(), origin.y());
    t.PreconcatTransform(transform);
    t.Translate(-origin.x(), -origin.y());
    return t;
  }

  gfx::RectF GetTransformedBounds(aura::Window* window) {
    gfx::RectF bounds(ash::ScreenAsh::ConvertRectToScreen(
        window->parent(), window->layer()->bounds()));
    gfx::Transform transform(GetTransformRelativeTo(bounds.origin(),
        window->layer()->transform()));
    transform.TransformRect(&bounds);
    return bounds;
  }

  gfx::RectF GetTransformedTargetBounds(aura::Window* window) {
    gfx::RectF bounds(ash::ScreenAsh::ConvertRectToScreen(
        window->parent(), window->layer()->GetTargetBounds()));
    gfx::Transform transform(GetTransformRelativeTo(bounds.origin(),
        window->layer()->GetTargetTransform()));
    transform.TransformRect(&bounds);
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
        Shell::GetPrimaryRootWindow())->GetFocusedWindow();
  }

  test::LauncherViewTestAPI* launcher_view_test() {
    return launcher_view_test_.get();
  }

 private:
  aura::test::TestWindowDelegate wd;
  scoped_ptr<test::LauncherViewTestAPI> launcher_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorTest);
};

// Tests entering overview mode with two windows and selecting one.
TEST_F(WindowSelectorTest, Basic) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(bounds));
  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));
  EXPECT_TRUE(WindowsOverlapping(panel1.get(), panel2.get()));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window2.get(), GetFocusedWindow());

  // In overview mode the windows should no longer overlap and focus should
  // be removed from the window.
  ToggleOverview();
  EXPECT_EQ(NULL, GetFocusedWindow());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  EXPECT_FALSE(WindowsOverlapping(window1.get(), panel1.get()));
  // Panels 1 and 2 should still be overlapping being in a single selector
  // item.
  EXPECT_TRUE(WindowsOverlapping(panel1.get(), panel2.get()));

  // The cursor should be locked as a pointer
  EXPECT_EQ(ui::kCursorPointer, root_window->last_cursor().native_type());
  EXPECT_TRUE(GetCursorClient(root_window)->IsCursorLocked());

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window1.get(), GetFocusedWindow());

  // Cursor should have been unlocked.
  EXPECT_FALSE(GetCursorClient(root_window)->IsCursorLocked());
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

// Tests cycles between panel and normal windows.
TEST_F(WindowSelectorTest, CyclePanels) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(panel2.get());
  wm::ActivateWindow(panel1.get());
  EXPECT_TRUE(wm::IsActiveWindow(panel1.get()));

  // Cycling once should select window1 since the panels are grouped into a
  // single selectable item.
  Cycle(WindowSelector::FORWARD);
  StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Cycling again should select the most recently used panel.
  Cycle(WindowSelector::FORWARD);
  StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel1.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowSelectorTest, CyclePanelsDestroyed) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> window3(CreateWindow(bounds));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(panel2.get());
  wm::ActivateWindow(panel1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Cycling once highlights window2.
  Cycle(WindowSelector::FORWARD);
  // All panels are destroyed.
  panel1.reset();
  panel2.reset();
  // Cycling again should now select window3.
  Cycle(WindowSelector::FORWARD);
  StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));
}

// Tests cycles between panel and normal windows.
TEST_F(WindowSelectorTest, CycleMruPanelDestroyed) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(bounds));
  wm::ActivateWindow(panel2.get());
  wm::ActivateWindow(panel1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Cycling once highlights window2.
  Cycle(WindowSelector::FORWARD);
  // Panel 1 is the next item as the MRU panel, removing it should make panel 2
  // the next window to be selected.
  panel1.reset();
  // Cycling again should now select window3.
  Cycle(WindowSelector::FORWARD);
  StopCycling();
  EXPECT_TRUE(wm::IsActiveWindow(panel2.get()));
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

  UpdateDisplay("600x400,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  gfx::Rect bounds1(0, 0, 400, 400);
  gfx::Rect bounds2(650, 0, 400, 400);

  scoped_ptr<aura::Window> window1(CreateWindow(bounds1));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds1));
  scoped_ptr<aura::Window> window3(CreateWindow(bounds2));
  scoped_ptr<aura::Window> window4(CreateWindow(bounds2));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds1));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(bounds1));
  scoped_ptr<aura::Window> panel3(CreatePanelWindow(bounds2));
  scoped_ptr<aura::Window> panel4(CreatePanelWindow(bounds2));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());

  EXPECT_EQ(root_windows[0], panel1->GetRootWindow());
  EXPECT_EQ(root_windows[0], panel2->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel3->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel4->GetRootWindow());

  // In overview mode, each window remains in the same root window.
  ToggleOverview();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());
  EXPECT_EQ(root_windows[0], panel1->GetRootWindow());
  EXPECT_EQ(root_windows[0], panel2->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel3->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel4->GetRootWindow());

  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(window1.get()))));
  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(window2.get()))));
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(window3.get()))));
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(window4.get()))));

  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(panel1.get()))));
  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(panel2.get()))));
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(panel3.get()))));
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(panel4.get()))));
  EXPECT_TRUE(WindowsOverlapping(panel1.get(), panel2.get()));
  EXPECT_TRUE(WindowsOverlapping(panel3.get(), panel4.get()));
  EXPECT_FALSE(WindowsOverlapping(panel1.get(), panel3.get()));
}

// Verifies that the single display overview used during alt tab cycling uses
// the display of the currently selected window.
TEST_F(WindowSelectorTest, CycleOverviewUsesCurrentDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> window1(CreateWindow(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> window2(CreateWindow(gfx::Rect(450, 0, 100, 100)));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(root_windows[0], Shell::GetTargetRootWindow());

  Cycle(WindowSelector::FORWARD);
  FireOverviewStartTimer();

  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(window1.get()))));
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      ToEnclosingRect(GetTransformedTargetBounds(window2.get()))));
}

}  // namespace internal
}  // namespace ash
