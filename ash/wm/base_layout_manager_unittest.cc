// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_layout_manager.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

class BaseLayoutManagerTest : public test::AshTestBase {
 public:
  BaseLayoutManagerTest() {}
  virtual ~BaseLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    Shell::GetInstance()->SetDisplayWorkAreaInsets(
        Shell::GetPrimaryRootWindow(),
        gfx::Insets(1, 2, 3, 4));
    Shell::GetPrimaryRootWindow()->SetHostSize(gfx::Size(800, 600));
    aura::Window* default_container = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        internal::kShellWindowId_DefaultContainer);
    default_container->SetLayoutManager(new internal::BaseLayoutManager(
        Shell::GetPrimaryRootWindow()));
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithBounds(bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseLayoutManagerTest);
};

// Tests normal->maximize->normal.
TEST_F(BaseLayoutManagerTest, Maximize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  // Maximized window fills the work area, not the whole display.
  EXPECT_EQ(
      ScreenAsh::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Tests normal->minimize->normal.
TEST_F(BaseLayoutManagerTest, Minimize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  // Note: Currently minimize doesn't do anything except set the state.
  // See crbug.com/104571.
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// A WindowDelegate which sets the focus when the window
// becomes visible.
class FocusDelegate : public aura::test::TestWindowDelegate {
 public:
  FocusDelegate()
      : window_(NULL),
        show_state_(ui::SHOW_STATE_END) {
  }
  virtual ~FocusDelegate() {}

  void set_window(aura::Window* window) { window_ = window; }

  // aura::test::TestWindowDelegate overrides:
  virtual void OnWindowTargetVisibilityChanged(bool visible) {
    if (window_) {
      if (visible)
        window_->Focus();
      show_state_ = window_->GetProperty(aura::client::kShowStateKey);
    }
  }

  ui::WindowShowState GetShowStateAndReset() {
    ui::WindowShowState ret = show_state_;
    show_state_ = ui::SHOW_STATE_END;
    return ret;
  }

 private:
  aura::Window* window_;
  ui::WindowShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(FocusDelegate);
};

// Make sure that the window's show state is correct in
// |WindowDelegate::OnWindowTargetVisibilityChanged|, and setting
// focus in this callback doesn't cause DCHECK error.  See
// crbug.com/168383.
TEST_F(BaseLayoutManagerTest, FocusDuringUnminimize) {
  FocusDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(100, 100, 100, 100)));
  delegate.set_window(window.get());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED, delegate.GetShowStateAndReset());
  window->Show();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_DEFAULT, delegate.GetShowStateAndReset());
}

// Tests maximized window size during root window resize.
TEST_F(BaseLayoutManagerTest, MaximizeRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  gfx::Rect initial_work_area_bounds =
      ScreenAsh::GetMaximizedWindowBoundsInParent(window.get());
  EXPECT_EQ(initial_work_area_bounds.ToString(), window->bounds().ToString());
  // Enlarge the root window.  We should still match the work area size.
  Shell::GetPrimaryRootWindow()->SetHostSize(gfx::Size(900, 700));
  EXPECT_EQ(
      ScreenAsh::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  EXPECT_NE(
      initial_work_area_bounds.ToString(),
      ScreenAsh::GetMaximizedWindowBoundsInParent(window.get()).ToString());
}

// Tests normal->fullscreen->normal.
TEST_F(BaseLayoutManagerTest, Fullscreen) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  // Fullscreen window fills the whole display.
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Tests fullscreen window size during root window resize.
TEST_F(BaseLayoutManagerTest, FullscreenRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  // Fullscreen window fills the whole display.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
  // Enlarge the root window.  We should still match the display size.
  Shell::GetPrimaryRootWindow()->SetHostSize(gfx::Size(800, 600));
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
}

// Fails on Mac only.  Need to be implemented.  http://crbug.com/111279.
#if defined(OS_MACOSX)
#define MAYBE_RootWindowResizeShrinksWindows \
  DISABLED_RootWindowResizeShrinksWindows
#else
#define MAYBE_RootWindowResizeShrinksWindows RootWindowResizeShrinksWindows
#endif
// Tests that when the screen gets smaller the windows aren't bigger than
// the screen.
TEST_F(BaseLayoutManagerTest, MAYBE_RootWindowResizeShrinksWindows) {
  scoped_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 500, 400)));
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  // Invariant: Window is smaller than work area.
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window narrower than our window.
  Shell::GetPrimaryRootWindow()->SetHostSize(gfx::Size(300, 400));
  work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window shorter than our window.
  Shell::GetPrimaryRootWindow()->SetHostSize(gfx::Size(300, 200));
  work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Enlarging the root window does not change the window bounds.
  gfx::Rect old_bounds = window->bounds();
  Shell::GetPrimaryRootWindow()->SetHostSize(gfx::Size(800, 600));
  EXPECT_EQ(old_bounds.width(), window->bounds().width());
  EXPECT_EQ(old_bounds.height(), window->bounds().height());
}

// Tests that a maximized window with too-large restore bounds will be restored
// to smaller than the full work area.
TEST_F(BaseLayoutManagerTest, BoundsWithScreenEdgeVisible) {
  // Create a window with bounds that fill the screen.
  gfx::Rect bounds = Shell::GetScreen()->GetPrimaryDisplay().bounds();
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  // Maximize it, which writes the old bounds to restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  // Restore it.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  // It should have the default maximized window bounds, inset by the grid size.
  int grid_size = internal::WorkspaceWindowResizer::kScreenEdgeInset;
  gfx::Rect max_bounds =
      ash::ScreenAsh::GetMaximizedWindowBoundsInParent(window.get());
  max_bounds.Inset(grid_size, grid_size);
  EXPECT_EQ(max_bounds.ToString(), window->bounds().ToString());
}

// Verifies maximizing sets the restore bounds, and restoring
// restores the bounds.
TEST_F(BaseLayoutManagerTest, MaximizeSetsRestoreBounds) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("1,2 3x4", GetRestoreBoundsInParent(window.get()).ToString());

  // Restore it, which should restore bounds and reset restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ("1,2 3x4", window->bounds().ToString());
  EXPECT_TRUE(GetRestoreBoundsInScreen(window.get()) == NULL);
}

// Verifies maximizing keeps the restore bounds if set.
TEST_F(BaseLayoutManagerTest, MaximizeResetsRestoreBounds) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  SetRestoreBoundsInParent(window.get(), gfx::Rect(10, 11, 12, 13));

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("10,11 12x13", GetRestoreBoundsInParent(window.get()).ToString());
}

// Verifies that the restore bounds do not get reset when restoring to a
// maximzied state from a minimized state.
TEST_F(BaseLayoutManagerTest, BoundsAfterRestoringToMaximizeFromMinimize) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  // Maximize it, which should reset restore bounds.
  wm::MaximizeWindow(window.get());
  EXPECT_EQ(bounds.ToString(),
            GetRestoreBoundsInParent(window.get()).ToString());

  // Minimize the window. The restore bounds should not change.
  wm::MinimizeWindow(window.get());
  EXPECT_EQ(bounds.ToString(),
            GetRestoreBoundsInParent(window.get()).ToString());

  // Show the window again. The window should be maximized, and the restore
  // bounds should not change.
  window->Show();
  EXPECT_EQ(bounds.ToString(),
            GetRestoreBoundsInParent(window.get()).ToString());
  EXPECT_TRUE(wm::IsWindowMaximized(window.get()));

  wm::RestoreWindow(window.get());
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

}  // namespace

}  // namespace ash
