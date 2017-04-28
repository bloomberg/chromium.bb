// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <string>
#include <utility>

#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/wm_shelf.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/shell_port.h"
#include "ash/test/ash_test.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/maximize_mode/workspace_backdrop_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_screen_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ash/wm_window.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

class MaximizeDelegateView : public views::WidgetDelegateView {
 public:
  explicit MaximizeDelegateView(const gfx::Rect& initial_bounds)
      : initial_bounds_(initial_bounds) {}
  ~MaximizeDelegateView() override {}

  bool GetSavedWindowPlacement(const views::Widget* widget,
                               gfx::Rect* bounds,
                               ui::WindowShowState* show_state) const override {
    *bounds = initial_bounds_;
    *show_state = ui::SHOW_STATE_MAXIMIZED;
    return true;
  }

 private:
  const gfx::Rect initial_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeDelegateView);
};

class TestShellObserver : public ShellObserver {
 public:
  TestShellObserver() : call_count_(0), is_fullscreen_(false) {
    Shell::Get()->AddShellObserver(this);
  }

  ~TestShellObserver() override { Shell::Get()->RemoveShellObserver(this); }

  void OnFullscreenStateChanged(bool is_fullscreen,
                                WmWindow* root_window) override {
    call_count_++;
    is_fullscreen_ = is_fullscreen;
  }

  int call_count() const { return call_count_; }

  bool is_fullscreen() const { return is_fullscreen_; }

 private:
  int call_count_;
  bool is_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(TestShellObserver);
};

}  // namespace

using WorkspaceLayoutManagerTest = AshTest;

// Verifies that a window containing a restore coordinate will be restored to
// to the size prior to minimize, keeping the restore rectangle in tact (if
// there is one).
TEST_F(WorkspaceLayoutManagerTest, RestoreFromMinimizeKeepsRestore) {
  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  WmWindow* window = window_owner->window();
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  wm::WindowState* window_state = window->GetWindowState();

  // This will not be used for un-minimizing window.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(0, 0, 100, 100));
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ("0,0 100x100", window_state->GetRestoreBoundsInScreen().ToString());
  EXPECT_EQ("10,15 25x35", window->GetBounds().ToString());

  UpdateDisplay("400x300,500x400");
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100), GetSecondaryDisplay());
  EXPECT_EQ(ShellPort::Get()->GetAllRootWindows()[1], window->GetRootWindow());
  window_state->Minimize();
  // This will not be used for un-minimizing window.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(0, 0, 100, 100));
  window_state->Restore();
  EXPECT_EQ("600,0 100x100", window->GetBoundsInScreen().ToString());

  // Make sure the unminimized window moves inside the display when
  // 2nd display is disconnected.
  window_state->Minimize();
  UpdateDisplay("400x300");
  window_state->Restore();
  EXPECT_EQ(ShellPort::Get()->GetPrimaryRootWindow(), window->GetRootWindow());
  EXPECT_TRUE(ShellPort::Get()->GetPrimaryRootWindow()->GetBounds().Intersects(
      window->GetBounds()));
}

TEST_F(WorkspaceLayoutManagerTest, KeepMinimumVisibilityInDisplays) {
  UpdateDisplay("300x400,400x500");
  WmWindow::Windows root_windows = ShellPort::Get()->GetAllRootWindows();

  if (!SetSecondaryDisplayPlacement(display::DisplayPlacement::TOP, 0))
    return;

  EXPECT_EQ("0,-500 400x500", root_windows[1]->GetBoundsInScreen().ToString());

  std::unique_ptr<WindowOwner> window1_owner(
      CreateTestWindow(gfx::Rect(10, -400, 200, 200)));
  EXPECT_EQ("10,-400 200x200",
            window1_owner->window()->GetBoundsInScreen().ToString());

  // Make sure the caption is visible.
  std::unique_ptr<WindowOwner> window2_owner(
      CreateTestWindow(gfx::Rect(10, -600, 200, 200)));
  EXPECT_EQ("10,-500 200x200",
            window2_owner->window()->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, NoMinimumVisibilityForPopupWindows) {
  UpdateDisplay("300x400");

  // Create a popup window out of display boundaries and make sure it is not
  // moved to have minimum visibility.
  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(gfx::Rect(400, 100, 50, 50), ui::wm::WINDOW_TYPE_POPUP));
  EXPECT_EQ("400,100 50x50",
            window_owner->window()->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, KeepRestoredWindowInDisplay) {
  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  WmWindow* window = window_owner->window();
  wm::WindowState* window_state = window->GetWindowState();

  // Maximized -> Normal transition.
  window_state->Maximize();
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 30, 40));
  window_state->Restore();
  EXPECT_TRUE(ShellPort::Get()->GetPrimaryRootWindow()->GetBounds().Intersects(
      window->GetBounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-5,0 30x40", window->GetBounds().ToString());

  // Minimized -> Normal transition.
  window->SetBounds(gfx::Rect(-100, -100, 30, 40));
  window_state->Minimize();
  EXPECT_FALSE(ShellPort::Get()->GetPrimaryRootWindow()->GetBounds().Intersects(
      window->GetBounds()));
  EXPECT_EQ("-100,-100 30x40", window->GetBounds().ToString());
  window->Show();
  EXPECT_TRUE(ShellPort::Get()->GetPrimaryRootWindow()->GetBounds().Intersects(
      window->GetBounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-5,0 30x40", window->GetBounds().ToString());

  // Fullscreen -> Normal transition.
  window->SetBounds(gfx::Rect(0, 0, 30, 40));  // reset bounds.
  ASSERT_EQ("0,0 30x40", window->GetBounds().ToString());
  window->SetShowState(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(window->GetBounds(), window->GetRootWindow()->GetBounds());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 30, 40));
  window_state->Restore();
  EXPECT_TRUE(ShellPort::Get()->GetPrimaryRootWindow()->GetBounds().Intersects(
      window->GetBounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-5,0 30x40", window->GetBounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, MaximizeInDisplayToBeRestored) {
  UpdateDisplay("300x400,400x500");

  WmWindow::Windows root_windows = ShellPort::Get()->GetAllRootWindows();

  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  WmWindow* window = window_owner->window();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = window->GetWindowState();
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(gfx::Rect(300, 0, 400, 500 - kShelfSize).ToString(),
            window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 30x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(295, 0, 30, 40));
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(gfx::Rect(300, 0, 400, 500 - kShelfSize).ToString(),
            window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("295,0 30x40", window->GetBoundsInScreen().ToString());

  // Restoring widget state.
  std::unique_ptr<views::Widget> w1(new views::Widget);
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = new MaximizeDelegateView(gfx::Rect(400, 0, 30, 40));
  ConfigureWidgetInitParamsForDisplay(root_windows[0], &params);
  w1->Init(params);
  EXPECT_EQ(root_windows[0],
            WmWindow::Get(w1->GetNativeWindow())->GetRootWindow());
  w1->Show();
  EXPECT_TRUE(w1->IsMaximized());
  EXPECT_EQ(root_windows[1],
            WmWindow::Get(w1->GetNativeWindow())->GetRootWindow());
  EXPECT_EQ(gfx::Rect(300, 0, 400, 500 - kShelfSize).ToString(),
            w1->GetWindowBoundsInScreen().ToString());
  w1->Restore();
  EXPECT_EQ(root_windows[1],
            WmWindow::Get(w1->GetNativeWindow())->GetRootWindow());
  EXPECT_EQ("400,0 30x40", w1->GetWindowBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, FullscreenInDisplayToBeRestored) {
  UpdateDisplay("300x400,400x500");

  WmWindow::Windows root_windows = ShellPort::Get()->GetAllRootWindows();

  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  WmWindow* window = window_owner->window();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = window->GetWindowState();
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window->SetShowState(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 30x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(295, 0, 30, 40));
  window->SetShowState(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("295,0 30x40", window->GetBoundsInScreen().ToString());
}

// aura::WindowObserver implementation used by
// DontClobberRestoreBoundsWindowObserver. This code mirrors what
// BrowserFrameAsh does. In particular when this code sees the window was
// maximized it changes the bounds of a secondary window. The secondary window
// mirrors the status window.
class DontClobberRestoreBoundsWindowObserver : public aura::WindowObserver {
 public:
  DontClobberRestoreBoundsWindowObserver() : window_(nullptr) {}

  void set_window(WmWindow* window) { window_ = window; }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (!window_)
      return;

    if (wm::GetWindowState(window)->IsMaximized()) {
      WmWindow* w = window_;
      window_ = nullptr;

      gfx::Rect shelf_bounds(AshTest::GetPrimaryShelf()->GetIdealBounds());
      const gfx::Rect& window_bounds(w->GetBounds());
      w->SetBounds(gfx::Rect(window_bounds.x(), shelf_bounds.y() - 1,
                             window_bounds.width(), window_bounds.height()));
    }
  }

 private:
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(DontClobberRestoreBoundsWindowObserver);
};

// Creates a window, maximized the window and from within the maximized
// notification sets the bounds of a window to overlap the shelf. Verifies this
// doesn't effect the restore bounds.
TEST_F(WorkspaceLayoutManagerTest, DontClobberRestoreBounds) {
  DontClobberRestoreBoundsWindowObserver window_observer;
  std::unique_ptr<aura::Window> window(
      base::MakeUnique<aura::Window>(nullptr, ui::wm::WINDOW_TYPE_NORMAL));
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(10, 20, 30, 40));
  // NOTE: for this test to exercise the failure the observer needs to be added
  // before the parent set. This mimics what BrowserFrameAsh does.
  window->AddObserver(&window_observer);
  ParentWindowInPrimaryRootWindow(WmWindow::Get(window.get()));
  window->Show();

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->Activate();

  std::unique_ptr<WindowOwner> window2_owner(
      CreateTestWindow(gfx::Rect(12, 20, 30, 40)));
  WmWindow* window2 = window2_owner->window();
  AddTransientChild(WmWindow::Get(window.get()), window2);
  window2->Show();

  window_observer.set_window(window2);
  window_state->Maximize();
  EXPECT_EQ("10,20 30x40", window_state->GetRestoreBoundsInScreen().ToString());
  window->RemoveObserver(&window_observer);
}

// Verifies when a window is maximized all descendant windows have a size.
TEST_F(WorkspaceLayoutManagerTest, ChildBoundsResetOnMaximize) {
  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(gfx::Rect(10, 20, 30, 40)));
  WmWindow* window = window_owner->window();
  window->Show();
  wm::WindowState* window_state = window->GetWindowState();
  window_state->Activate();
  std::unique_ptr<WindowOwner> child_window_owner(
      CreateChildWindow(window, gfx::Rect(5, 6, 7, 8)));
  WmWindow* child_window = child_window_owner->window();
  window_state->Maximize();
  EXPECT_EQ("5,6 7x8", child_window->GetBounds().ToString());
}

// Verifies a window created with maximized state has the maximized
// bounds.
TEST_F(WorkspaceLayoutManagerTest, MaximizeWithEmptySize) {
  std::unique_ptr<aura::Window> window(
      base::MakeUnique<aura::Window>(nullptr, ui::wm::WINDOW_TYPE_NORMAL));
  window->Init(ui::LAYER_TEXTURED);
  wm::GetWindowState(window.get())->Maximize();
  WmWindow* default_container =
      ShellPort::Get()->GetPrimaryRootWindowController()->GetWmContainer(
          kShellWindowId_DefaultContainer);
  default_container->aura_window()->AddChild(window.get());
  window->Show();
  gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
  EXPECT_EQ(work_area.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, WindowShouldBeOnScreenWhenAdded) {
  // TODO: fix. This test verifies that when a window is added the bounds are
  // adjusted. CreateTestWindow() for mus adds, then sets the bounds (this comes
  // from NativeWidgetAura), which means this test now fails for aura-mus.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  // Normal window bounds shouldn't be changed.
  gfx::Rect window_bounds(100, 100, 200, 200);
  std::unique_ptr<WindowOwner> window_owner(CreateTestWindow(window_bounds));
  WmWindow* window = window_owner->window();
  EXPECT_EQ(window_bounds, window->GetBounds());

  // If the window is out of the workspace, it would be moved on screen.
  gfx::Rect root_window_bounds =
      ShellPort::Get()->GetPrimaryRootWindow()->GetBounds();
  window_bounds.Offset(root_window_bounds.width(), root_window_bounds.height());
  ASSERT_FALSE(window_bounds.Intersects(root_window_bounds));
  std::unique_ptr<WindowOwner> out_window_owner(
      CreateTestWindow(window_bounds));
  WmWindow* out_window = out_window_owner->window();
  EXPECT_EQ(window_bounds.size(), out_window->GetBounds().size());
  gfx::Rect bounds = out_window->GetBounds();
  bounds.Intersect(root_window_bounds);

  // 30% of the window edge must be visible.
  EXPECT_GT(bounds.width(), out_window->GetBounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->GetBounds().height() * 0.29);

  WmWindow* parent = out_window->GetParent();
  parent->RemoveChild(out_window);
  out_window->SetBounds(gfx::Rect(-200, -200, 200, 200));
  // UserHasChangedWindowPositionOrSize flag shouldn't turn off this behavior.
  window->GetWindowState()->set_bounds_changed_by_user(true);
  parent->AddChild(out_window);
  EXPECT_GT(bounds.width(), out_window->GetBounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->GetBounds().height() * 0.29);

  // Make sure we always make more than 1/3 of the window edge visible even
  // if the initial bounds intersects with display.
  window_bounds.SetRect(-150, -150, 200, 200);
  bounds = window_bounds;
  bounds.Intersect(root_window_bounds);

  // Make sure that the initial bounds' visible area is less than 26%
  // so that the auto adjustment logic kicks in.
  ASSERT_LT(bounds.width(), out_window->GetBounds().width() * 0.26);
  ASSERT_LT(bounds.height(), out_window->GetBounds().height() * 0.26);
  ASSERT_TRUE(window_bounds.Intersects(root_window_bounds));

  std::unique_ptr<WindowOwner> partially_out_window_owner(
      CreateTestWindow(window_bounds));
  WmWindow* partially_out_window = partially_out_window_owner->window();
  EXPECT_EQ(window_bounds.size(), partially_out_window->GetBounds().size());
  bounds = partially_out_window->GetBounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->GetBounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->GetBounds().height() * 0.29);

  // Make sure the window whose 30% width/height is bigger than display
  // will be placed correctly.
  window_bounds.SetRect(-1900, -1900, 3000, 3000);
  std::unique_ptr<WindowOwner> window_bigger_than_display_owner(
      CreateTestWindow(window_bounds));
  WmWindow* window_bigger_than_display =
      window_bigger_than_display_owner->window();
  EXPECT_GE(root_window_bounds.width(),
            window_bigger_than_display->GetBounds().width());
  EXPECT_GE(root_window_bounds.height(),
            window_bigger_than_display->GetBounds().height());

  bounds = window_bigger_than_display->GetBounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->GetBounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->GetBounds().height() * 0.29);
}

// Verifies the size of a window is enforced to be smaller than the work area.
TEST_F(WorkspaceLayoutManagerTest, SizeToWorkArea) {
  // Normal window bounds shouldn't be changed.
  gfx::Size work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().size());
  const gfx::Rect window_bounds(100, 101, work_area.width() + 1,
                                work_area.height() + 2);
  std::unique_ptr<WindowOwner> window_owner(CreateTestWindow(window_bounds));
  WmWindow* window = window_owner->window();
  // TODO: fix. This test verifies that when a window is added the bounds are
  // adjusted. CreateTestWindow() for mus adds, then sets the bounds (this comes
  // from NativeWidgetAura), which means this test now fails for aura-mus.
  if (Shell::GetAshConfig() == Config::CLASSIC) {
    EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
              window->GetBounds().ToString());
  }

  // Directly setting the bounds triggers a slightly different code path. Verify
  // that too.
  window->SetBounds(window_bounds);
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
            window->GetBounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, NotifyFullscreenChanges) {
  TestShellObserver observer;
  std::unique_ptr<WindowOwner> window1_owner(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  WmWindow* window1 = window1_owner->window();
  std::unique_ptr<WindowOwner> window2_owner(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  WmWindow* window2 = window2_owner->window();
  wm::WindowState* window_state1 = window1->GetWindowState();
  wm::WindowState* window_state2 = window2->GetWindowState();
  window_state2->Activate();

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state2->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_EQ(1, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  // When window1 moves to the front the fullscreen state should change.
  window_state1->Activate();
  EXPECT_EQ(2, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());

  // It should change back if window2 becomes active again.
  window_state2->Activate();
  EXPECT_EQ(3, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  window_state2->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_EQ(4, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());

  window_state2->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_EQ(5, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  // Closing the window should change the fullscreen state.
  window2_owner.reset();
  EXPECT_EQ(6, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());
}

// For crbug.com/673803, snapped window may not adjust snapped bounds on work
// area changed properly if window's layer is doing animation. We should use
// GetTargetBounds to check if snapped bounds need to be changed.
TEST_F(WorkspaceLayoutManagerTest,
       SnappedWindowMayNotAdjustBoundsOnWorkAreaChanged) {
  UpdateDisplay("300x400");
  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(gfx::Rect(10, 20, 100, 200)));
  WmWindow* window = window_owner->window();
  wm::WindowState* window_state = window->GetWindowState();
  gfx::Insets insets(0, 0, 50, 0);
  ShellPort::Get()->SetDisplayWorkAreaInsets(window, insets);
  const wm::WMEvent snap_left(wm::WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&snap_left);
  EXPECT_EQ(wm::WINDOW_STATE_TYPE_LEFT_SNAPPED, window_state->GetStateType());
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected_bounds =
      gfx::Rect(kWorkAreaBounds.x(), kWorkAreaBounds.y(),
                kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  EXPECT_EQ(expected_bounds.ToString(), window->GetBounds().ToString());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // The following two SetDisplayWorkAreaInsets calls simulate the case of
  // crbug.com/673803 that work area first becomes fullscreen and then returns
  // to the original state.
  ShellPort::Get()->SetDisplayWorkAreaInsets(window, gfx::Insets(0, 0, 0, 0));
  ui::LayerAnimator* animator = window->GetLayer()->GetAnimator();
  EXPECT_TRUE(animator->is_animating());
  ShellPort::Get()->SetDisplayWorkAreaInsets(window, insets);
  animator->StopAnimating();
  EXPECT_FALSE(animator->is_animating());
  EXPECT_EQ(expected_bounds.ToString(), window->GetBounds().ToString());
}

// Do not adjust window bounds to ensure minimum visibility for transient
// windows (crbug.com/624806).
TEST_F(WorkspaceLayoutManagerTest,
       DoNotAdjustTransientWindowBoundsToEnsureMinimumVisibility) {
  UpdateDisplay("300x400");
  std::unique_ptr<aura::Window> window(
      base::MakeUnique<aura::Window>(nullptr, ui::wm::WINDOW_TYPE_NORMAL));
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(10, 0, 100, 200));
  ParentWindowInPrimaryRootWindow(WmWindow::Get(window.get()));
  window->Show();

  std::unique_ptr<WindowOwner> window2_owner(
      CreateTestWindow(gfx::Rect(10, 0, 40, 20)));
  WmWindow* window2 = window2_owner->window();
  AddTransientChild(WmWindow::Get(window.get()), window2);
  window2->Show();

  gfx::Rect expected_bounds = window2->GetBounds();
  ShellPort::Get()->SetDisplayWorkAreaInsets(WmWindow::Get(window.get()),
                                             gfx::Insets(50, 0, 0, 0));
  EXPECT_EQ(expected_bounds.ToString(), window2->GetBounds().ToString());
}

// Following "Solo" tests were originally written for BaseLayoutManager.
using WorkspaceLayoutManagerSoloTest = test::AshTestBase;

// Tests normal->maximize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Maximize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* window = WmWindow::Get(window_owner.get());
  window->SetShowState(ui::SHOW_STATE_MAXIMIZED);
  // Maximized window fills the work area, not the whole display.
  EXPECT_EQ(wm::GetMaximizedWindowBoundsInParent(window).ToString(),
            window->GetBounds().ToString());
  window->SetShowState(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->GetBounds().ToString());
}

// Tests normal->minimize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Minimize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* window = WmWindow::Get(window_owner.get());
  window->SetShowState(ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(window->GetWindowState()->IsMinimized());
  EXPECT_EQ(bounds, window->GetBounds());
  window->SetShowState(ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->GetWindowState()->IsMinimized());
  EXPECT_EQ(bounds, window->GetBounds());
}

// A aura::WindowObserver which sets the focus when the window becomes visible.
class FocusDuringUnminimizeWindowObserver : public aura::WindowObserver {
 public:
  FocusDuringUnminimizeWindowObserver()
      : window_(nullptr), show_state_(ui::SHOW_STATE_END) {}
  ~FocusDuringUnminimizeWindowObserver() override { SetWindow(nullptr); }

  void SetWindow(WmWindow* window) {
    if (window_)
      window_->aura_window()->RemoveObserver(this);
    window_ = window;
    if (window_)
      window_->aura_window()->AddObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window_) {
      if (visible)
        window_->SetFocused();
      show_state_ = window_->GetShowState();
    }
  }

  ui::WindowShowState GetShowStateAndReset() {
    ui::WindowShowState ret = show_state_;
    show_state_ = ui::SHOW_STATE_END;
    return ret;
  }

 private:
  WmWindow* window_;
  ui::WindowShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(FocusDuringUnminimizeWindowObserver);
};

// Make sure that the window's show state is correct in
// WindowObserver::OnWindowTargetVisibilityChanged(), and setting focus in this
// callback doesn't cause DCHECK error.  See crbug.com/168383.
TEST_F(WorkspaceLayoutManagerSoloTest, FocusDuringUnminimize) {
  FocusDuringUnminimizeWindowObserver observer;
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WmWindow* window = WmWindow::Get(window_owner.get());
  observer.SetWindow(window);
  window->SetShowState(ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED, observer.GetShowStateAndReset());
  window->Show();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, observer.GetShowStateAndReset());
  observer.SetWindow(nullptr);
}

// Tests maximized window size during root window resize.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* window = WmWindow::Get(window_owner.get());
  window->SetShowState(ui::SHOW_STATE_MAXIMIZED);
  gfx::Rect initial_work_area_bounds =
      wm::GetMaximizedWindowBoundsInParent(window);
  EXPECT_EQ(initial_work_area_bounds.ToString(),
            window->GetBounds().ToString());
  // Enlarge the root window.  We should still match the work area size.
  UpdateDisplay("900x700");
  EXPECT_EQ(wm::GetMaximizedWindowBoundsInParent(window).ToString(),
            window->GetBounds().ToString());
  EXPECT_NE(initial_work_area_bounds.ToString(),
            wm::GetMaximizedWindowBoundsInParent(window).ToString());
}

// Tests normal->fullscreen->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Fullscreen) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* window = WmWindow::Get(window_owner.get());
  window->SetShowState(ui::SHOW_STATE_FULLSCREEN);
  // Fullscreen window fills the whole display.
  EXPECT_EQ(window->GetDisplayNearestWindow().bounds().ToString(),
            window->GetBounds().ToString());
  window->SetShowState(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->GetBounds().ToString());
}

// Tests that fullscreen window causes always_on_top windows to stack below.
TEST_F(WorkspaceLayoutManagerSoloTest, FullscreenSuspendsAlwaysOnTop) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> fullscreen_window_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* fullscreen_window = WmWindow::Get(fullscreen_window_owner.get());
  std::unique_ptr<aura::Window> always_on_top_window1_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* always_on_top_window1 =
      WmWindow::Get(always_on_top_window1_owner.get());
  std::unique_ptr<aura::Window> always_on_top_window2_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* always_on_top_window2 =
      WmWindow::Get(always_on_top_window2_owner.get());
  always_on_top_window1->SetAlwaysOnTop(true);
  always_on_top_window2->SetAlwaysOnTop(true);
  // Making a window fullscreen temporarily suspends always on top state.
  fullscreen_window->SetShowState(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_FALSE(always_on_top_window1->IsAlwaysOnTop());
  EXPECT_FALSE(always_on_top_window2->IsAlwaysOnTop());
  EXPECT_NE(nullptr, wm::GetWindowForFullscreenMode(fullscreen_window));

  // Adding a new always-on-top window is not affected by fullscreen.
  std::unique_ptr<aura::Window> always_on_top_window3_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* always_on_top_window3 =
      WmWindow::Get(always_on_top_window3_owner.get());
  always_on_top_window3->SetAlwaysOnTop(true);
  EXPECT_TRUE(always_on_top_window3->IsAlwaysOnTop());

  // Making fullscreen window normal restores always on top windows.
  fullscreen_window->SetShowState(ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(always_on_top_window1->IsAlwaysOnTop());
  EXPECT_TRUE(always_on_top_window2->IsAlwaysOnTop());
  EXPECT_TRUE(always_on_top_window3->IsAlwaysOnTop());
  EXPECT_EQ(nullptr, wm::GetWindowForFullscreenMode(fullscreen_window));
}

// Similary, pinned window causes always_on_top_ windows to stack below.
TEST_F(WorkspaceLayoutManagerSoloTest, PinnedSuspendsAlwaysOnTop) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> pinned_window_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* pinned_window = WmWindow::Get(pinned_window_owner.get());
  std::unique_ptr<aura::Window> always_on_top_window1_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* always_on_top_window1 =
      WmWindow::Get(always_on_top_window1_owner.get());
  std::unique_ptr<aura::Window> always_on_top_window2_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* always_on_top_window2 =
      WmWindow::Get(always_on_top_window2_owner.get());
  always_on_top_window1->SetAlwaysOnTop(true);
  always_on_top_window2->SetAlwaysOnTop(true);

  // Making a window pinned temporarily suspends always on top state.
  const bool trusted = false;
  wm::PinWindow(pinned_window->aura_window(), trusted);
  EXPECT_FALSE(always_on_top_window1->IsAlwaysOnTop());
  EXPECT_FALSE(always_on_top_window2->IsAlwaysOnTop());

  // Adding a new always-on-top window also is affected by pinned mode.
  std::unique_ptr<aura::Window> always_on_top_window3_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* always_on_top_window3 =
      WmWindow::Get(always_on_top_window3_owner.get());
  always_on_top_window3->SetAlwaysOnTop(true);
  EXPECT_FALSE(always_on_top_window3->IsAlwaysOnTop());

  // Making pinned window normal restores always on top windows.
  pinned_window->GetWindowState()->Restore();
  EXPECT_TRUE(always_on_top_window1->IsAlwaysOnTop());
  EXPECT_TRUE(always_on_top_window2->IsAlwaysOnTop());
  EXPECT_TRUE(always_on_top_window3->IsAlwaysOnTop());
}

// Tests fullscreen window size during root window resize.
TEST_F(WorkspaceLayoutManagerSoloTest, FullscreenRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(bounds));
  WmWindow* window = WmWindow::Get(window_owner.get());
  // Fullscreen window fills the whole display.
  window->SetShowState(ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(window->GetDisplayNearestWindow().bounds().ToString(),
            window->GetBounds().ToString());
  // Enlarge the root window.  We should still match the display size.
  UpdateDisplay("800x600");
  EXPECT_EQ(window->GetDisplayNearestWindow().bounds().ToString(),
            window->GetBounds().ToString());
}

// Tests that when the screen gets smaller the windows aren't bigger than
// the screen.
TEST_F(WorkspaceLayoutManagerSoloTest, RootWindowResizeShrinksWindows) {
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 500, 400)));
  WmWindow* window = WmWindow::Get(window_owner.get());
  gfx::Rect work_area = window->GetDisplayNearestWindow().work_area();
  // Invariant: Window is smaller than work area.
  EXPECT_LE(window->GetBounds().width(), work_area.width());
  EXPECT_LE(window->GetBounds().height(), work_area.height());

  // Make the root window narrower than our window.
  UpdateDisplay("300x400");
  work_area = window->GetDisplayNearestWindow().work_area();
  EXPECT_LE(window->GetBounds().width(), work_area.width());
  EXPECT_LE(window->GetBounds().height(), work_area.height());

  // Make the root window shorter than our window.
  UpdateDisplay("300x200");
  work_area = window->GetDisplayNearestWindow().work_area();
  EXPECT_LE(window->GetBounds().width(), work_area.width());
  EXPECT_LE(window->GetBounds().height(), work_area.height());

  // Enlarging the root window does not change the window bounds.
  gfx::Rect old_bounds = window->GetBounds();
  UpdateDisplay("800x600");
  EXPECT_EQ(old_bounds.width(), window->GetBounds().width());
  EXPECT_EQ(old_bounds.height(), window->GetBounds().height());
}

// Verifies maximizing sets the restore bounds, and restoring
// restores the bounds.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeSetsRestoreBounds) {
  const gfx::Rect initial_bounds(10, 20, 30, 40);
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(initial_bounds));
  WmWindow* window = WmWindow::Get(window_owner.get());
  EXPECT_EQ(initial_bounds, window->GetBounds());
  wm::WindowState* window_state = window->GetWindowState();

  // Maximize it, which will keep the previous restore bounds.
  window->SetShowState(ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("10,20 30x40", window_state->GetRestoreBoundsInParent().ToString());

  // Restore it, which should restore bounds and reset restore bounds.
  window->SetShowState(ui::SHOW_STATE_NORMAL);
  EXPECT_EQ("10,20 30x40", window->GetBounds().ToString());
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies maximizing keeps the restore bounds if set.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeResetsRestoreBounds) {
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  WmWindow* window = WmWindow::Get(window_owner.get());
  wm::WindowState* window_state = window->GetWindowState();
  window_state->SetRestoreBoundsInParent(gfx::Rect(10, 11, 12, 13));

  // Maximize it, which will keep the previous restore bounds.
  window->SetShowState(ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("10,11 12x13", window_state->GetRestoreBoundsInParent().ToString());
}

// Verifies that the restore bounds do not get reset when restoring to a
// maximzied state from a minimized state.
TEST_F(WorkspaceLayoutManagerSoloTest,
       BoundsAfterRestoringToMaximizeFromMinimize) {
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  WmWindow* window = WmWindow::Get(window_owner.get());
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  wm::WindowState* window_state = window->GetWindowState();
  // Maximize it, which should reset restore bounds.
  window_state->Maximize();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());
  // Minimize the window. The restore bounds should not change.
  window_state->Minimize();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());

  // Show the window again. The window should be maximized, and the restore
  // bounds should not change.
  window->Show();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());
  EXPECT_TRUE(window_state->IsMaximized());

  window_state->Restore();
  EXPECT_EQ(bounds.ToString(), window->GetBounds().ToString());
}

// Verify if the window is not resized during screen lock. See: crbug.com/173127
TEST_F(WorkspaceLayoutManagerSoloTest, NotResizeWhenScreenIsLocked) {
  SetCanLockScreen(true);
  std::unique_ptr<aura::Window> window_owner(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  WmWindow* window = WmWindow::Get(window_owner.get());
  // window with AlwaysOnTop will be managed by BaseLayoutManager.
  window->SetAlwaysOnTop(true);
  window->Show();

  WmShelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  window->SetBounds(wm::GetMaximizedWindowBoundsInParent(window));
  gfx::Rect window_bounds = window->GetBounds();
  EXPECT_EQ(wm::GetMaximizedWindowBoundsInParent(window).ToString(),
            window_bounds.ToString());

  // The window size should not get touched while we are in lock screen.
  Shell::Get()->session_controller()->LockScreenAndFlushForTest();
  ShelfLayoutManager* shelf_layout_manager = shelf->shelf_layout_manager();
  shelf_layout_manager->UpdateVisibilityState();
  EXPECT_EQ(window_bounds.ToString(), window->GetBounds().ToString());

  // Coming out of the lock screen the window size should still remain.
  GetSessionControllerClient()->UnlockScreen();
  shelf_layout_manager->UpdateVisibilityState();
  EXPECT_EQ(wm::GetMaximizedWindowBoundsInParent(window).ToString(),
            window_bounds.ToString());
  EXPECT_EQ(window_bounds.ToString(), window->GetBounds().ToString());
}

// Following tests are written to test the backdrop functionality.

namespace {

WorkspaceLayoutManager* GetWorkspaceLayoutManager(WmWindow* container) {
  return static_cast<WorkspaceLayoutManager*>(container->GetLayoutManager());
}

class WorkspaceLayoutManagerBackdropTest : public AshTest {
 public:
  WorkspaceLayoutManagerBackdropTest() : default_container_(nullptr) {}
  ~WorkspaceLayoutManagerBackdropTest() override {}

  void SetUp() override {
    AshTest::SetUp();
    UpdateDisplay("800x600");
    default_container_ =
        ShellPort::Get()->GetPrimaryRootWindowController()->GetWmContainer(
            kShellWindowId_DefaultContainer);
  }

  // Turn the top window back drop on / off.
  void ShowTopWindowBackdrop(bool show) {
    std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> backdrop;
    if (show)
      backdrop.reset(new WorkspaceBackdropDelegate(default_container_));
    GetWorkspaceLayoutManager(default_container_)
        ->SetMaximizeBackdropDelegate(std::move(backdrop));
    // Closing and / or opening can be a delayed operation.
    base::RunLoop().RunUntilIdle();
  }

  // Return the default container.
  WmWindow* default_container() { return default_container_; }

  // Return the order of windows (top most first) as they are in the default
  // container. If the window is visible it will be a big letter, otherwise a
  // small one. The backdrop will be an X and unknown windows will be shown as
  // '!'.
  std::string GetWindowOrderAsString(WmWindow* backdrop,
                                     WmWindow* wa,
                                     WmWindow* wb,
                                     WmWindow* wc) {
    std::string result;
    WmWindow::Windows children = default_container()->GetChildren();
    for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i) {
      if (!result.empty())
        result += ",";
      if (children[i] == wa)
        result += children[i]->IsVisible() ? "A" : "a";
      else if (children[i] == wb)
        result += children[i]->IsVisible() ? "B" : "b";
      else if (children[i] == wc)
        result += children[i]->IsVisible() ? "C" : "c";
      else if (children[i] == backdrop)
        result += children[i]->IsVisible() ? "X" : "x";
      else
        result += "!";
    }
    return result;
  }

 private:
  // The default container.
  WmWindow* default_container_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerBackdropTest);
};

}  // namespace

// Check that creating the BackDrop without destroying it does not lead into
// a crash.
TEST_F(WorkspaceLayoutManagerBackdropTest, BackdropCrashTest) {
  ShowTopWindowBackdrop(true);
}

// Verify basic assumptions about the backdrop.
TEST_F(WorkspaceLayoutManagerBackdropTest, BasicBackdropTests) {
  // Create a backdrop and see that there is one window (the backdrop) and
  // that the size is the same as the default container as well as that it is
  // not visible.
  ShowTopWindowBackdrop(true);
  ASSERT_EQ(1U, default_container()->GetChildren().size());
  EXPECT_FALSE(default_container()->GetChildren()[0]->IsVisible());

  {
    // Add a window and make sure that the backdrop is the second child.
    std::unique_ptr<WindowOwner> window_owner(
        CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
    WmWindow* window = window_owner->window();
    window->Show();
    ASSERT_EQ(2U, default_container()->GetChildren().size());
    EXPECT_TRUE(default_container()->GetChildren()[0]->IsVisible());
    EXPECT_TRUE(default_container()->GetChildren()[1]->IsVisible());
    EXPECT_EQ(window, default_container()->GetChildren()[1]);
    EXPECT_EQ(default_container()->GetBounds().ToString(),
              default_container()->GetChildren()[0]->GetBounds().ToString());
  }

  // With the window gone the backdrop should be invisible again.
  ASSERT_EQ(1U, default_container()->GetChildren().size());
  EXPECT_FALSE(default_container()->GetChildren()[0]->IsVisible());

  // Destroying the Backdrop should empty the container.
  ShowTopWindowBackdrop(false);
  ASSERT_EQ(0U, default_container()->GetChildren().size());
}

// Verify that the backdrop gets properly created and placed.
TEST_F(WorkspaceLayoutManagerBackdropTest, VerifyBackdropAndItsStacking) {
  std::unique_ptr<WindowOwner> window1_owner(
      CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  WmWindow* window1 = window1_owner->window();
  window1->Show();

  // Get the default container and check that only a single window is in there.
  ASSERT_EQ(1U, default_container()->GetChildren().size());
  EXPECT_EQ(window1, default_container()->GetChildren()[0]);
  EXPECT_EQ("A", GetWindowOrderAsString(nullptr, window1, nullptr, nullptr));

  // Create 2 more windows and check that they are also in the container.
  std::unique_ptr<WindowOwner> window2_owner(
      CreateTestWindow(gfx::Rect(10, 2, 3, 4)));
  WmWindow* window2 = window2_owner->window();
  std::unique_ptr<WindowOwner> window3_owner(
      CreateTestWindow(gfx::Rect(20, 2, 3, 4)));
  WmWindow* window3 = window3_owner->window();
  window2->Show();
  window3->Show();

  WmWindow* backdrop = nullptr;
  EXPECT_EQ("C,B,A",
            GetWindowOrderAsString(backdrop, window1, window2, window3));

  // Turn on the backdrop mode and check that the window shows up where it
  // should be (second highest number).
  ShowTopWindowBackdrop(true);
  backdrop = default_container()->GetChildren()[2];
  EXPECT_EQ("C,X,B,A",
            GetWindowOrderAsString(backdrop, window1, window2, window3));

  // Switch the order of windows and check that it still remains in that
  // location.
  default_container()->StackChildAtTop(window2);
  EXPECT_EQ("B,X,C,A",
            GetWindowOrderAsString(backdrop, window1, window2, window3));

  // Make the top window invisible and check.
  window2->Hide();
  EXPECT_EQ("b,C,X,A",
            GetWindowOrderAsString(backdrop, window1, window2, window3));
  // Then delete window after window and see that everything is in order.
  window1_owner.reset();
  EXPECT_EQ("b,C,X",
            GetWindowOrderAsString(backdrop, window1, window2, window3));
  window3_owner.reset();
  EXPECT_EQ("b,x", GetWindowOrderAsString(backdrop, window1, window2, window3));
  ShowTopWindowBackdrop(false);
  EXPECT_EQ("b", GetWindowOrderAsString(nullptr, window1, window2, window3));
}

// Tests that when hidding the shelf, that the backdrop stays fullscreen.
TEST_F(WorkspaceLayoutManagerBackdropTest,
       ShelfVisibilityDoesNotChangesBounds) {
  WmShelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* shelf_layout_manager = shelf->shelf_layout_manager();
  ShowTopWindowBackdrop(true);
  RunAllPendingInMessageLoop();
  const gfx::Size fullscreen_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();

  ASSERT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());
  EXPECT_EQ(fullscreen_size,
            default_container()->GetChildren()[0]->GetBounds().size());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  shelf_layout_manager->UpdateVisibilityState();

  // When the shelf is re-shown WorkspaceLayoutManager shrinks all children but
  // the backdrop.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf_layout_manager->UpdateVisibilityState();
  EXPECT_EQ(fullscreen_size,
            default_container()->GetChildren()[0]->GetBounds().size());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  shelf_layout_manager->UpdateVisibilityState();
  EXPECT_EQ(fullscreen_size,
            default_container()->GetChildren()[0]->GetBounds().size());
}

class WorkspaceLayoutManagerKeyboardTest : public AshTest {
 public:
  WorkspaceLayoutManagerKeyboardTest() : layout_manager_(nullptr) {}
  ~WorkspaceLayoutManagerKeyboardTest() override {}

  void SetUp() override {
    AshTest::SetUp();
    UpdateDisplay("800x600");
    WmWindow* default_container =
        ShellPort::Get()->GetPrimaryRootWindowController()->GetWmContainer(
            kShellWindowId_DefaultContainer);
    layout_manager_ = GetWorkspaceLayoutManager(default_container);
  }

  void ShowKeyboard() {
    layout_manager_->OnKeyboardBoundsChanging(keyboard_bounds_);
    restore_work_area_insets_ =
        display::Screen::GetScreen()->GetPrimaryDisplay().GetWorkAreaInsets();
    ShellPort::Get()->SetDisplayWorkAreaInsets(
        ShellPort::Get()->GetPrimaryRootWindow(),
        gfx::Insets(0, 0, keyboard_bounds_.height(), 0));
  }

  void HideKeyboard() {
    ShellPort::Get()->SetDisplayWorkAreaInsets(
        ShellPort::Get()->GetPrimaryRootWindow(), restore_work_area_insets_);
    layout_manager_->OnKeyboardBoundsChanging(gfx::Rect());
  }

  // Initializes the keyboard bounds using the bottom half of the work area.
  void InitKeyboardBounds() {
    gfx::Rect work_area(
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
    keyboard_bounds_.SetRect(work_area.x(),
                             work_area.y() + work_area.height() / 2,
                             work_area.width(), work_area.height() / 2);
  }

  void EnableNewVKMode() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(::switches::kUseNewVirtualKeyboardBehavior))
      command_line->AppendSwitch(::switches::kUseNewVirtualKeyboardBehavior);
  }

  const gfx::Rect& keyboard_bounds() const { return keyboard_bounds_; }

 private:
  gfx::Insets restore_work_area_insets_;
  gfx::Rect keyboard_bounds_;
  WorkspaceLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerKeyboardTest);
};

// Tests that when a child window gains focus the top level window containing it
// is resized to fit the remaining workspace area.
TEST_F(WorkspaceLayoutManagerKeyboardTest, ChildWindowFocused) {
  InitKeyboardBounds();

  gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  std::unique_ptr<WindowOwner> parent_window_owner(
      CreateToplevelTestWindow(work_area));
  WmWindow* parent_window = parent_window_owner->window();
  std::unique_ptr<WindowOwner> window_owner(CreateTestWindow(work_area));
  WmWindow* window = window_owner->window();
  parent_window->AddChild(window);

  window->Activate();

  int available_height =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().height() -
      keyboard_bounds().height();

  gfx::Rect initial_window_bounds(50, 50, 100, 500);
  parent_window->SetBounds(initial_window_bounds);
  EXPECT_EQ(initial_window_bounds.ToString(),
            parent_window->GetBounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(50, 0, 100, available_height).ToString(),
            parent_window->GetBounds().ToString());
  HideKeyboard();
  EXPECT_EQ(initial_window_bounds.ToString(),
            parent_window->GetBounds().ToString());
}

TEST_F(WorkspaceLayoutManagerKeyboardTest, AdjustWindowForA11yKeyboard) {
  InitKeyboardBounds();
  gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  std::unique_ptr<WindowOwner> window_owner(
      CreateToplevelTestWindow(work_area));
  WmWindow* window = window_owner->window();
  // The additional SetBounds() is needed as the aura-mus case uses Widget,
  // which alters the supplied bounds.
  window->SetBounds(work_area);

  int available_height =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().height() -
      keyboard_bounds().height();

  window->Activate();

  EXPECT_EQ(gfx::Rect(work_area).ToString(), window->GetBounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(work_area.origin(),
                      gfx::Size(work_area.width(), available_height))
                .ToString(),
            window->GetBounds().ToString());
  HideKeyboard();
  EXPECT_EQ(gfx::Rect(work_area).ToString(), window->GetBounds().ToString());

  gfx::Rect small_window_bound(50, 50, 100, 500);
  window->SetBounds(small_window_bound);
  EXPECT_EQ(small_window_bound.ToString(), window->GetBounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(50, 0, 100, available_height).ToString(),
            window->GetBounds().ToString());
  HideKeyboard();
  EXPECT_EQ(small_window_bound.ToString(), window->GetBounds().ToString());

  gfx::Rect occluded_window_bounds(
      50, keyboard_bounds().y() + keyboard_bounds().height() / 2, 50,
      keyboard_bounds().height() / 2);
  window->SetBounds(occluded_window_bounds);
  EXPECT_EQ(occluded_window_bounds.ToString(),
            occluded_window_bounds.ToString());
  ShowKeyboard();
  EXPECT_EQ(
      gfx::Rect(50, keyboard_bounds().y() - keyboard_bounds().height() / 2,
                occluded_window_bounds.width(), occluded_window_bounds.height())
          .ToString(),
      window->GetBounds().ToString());
  HideKeyboard();
  EXPECT_EQ(occluded_window_bounds.ToString(), window->GetBounds().ToString());
}

TEST_F(WorkspaceLayoutManagerKeyboardTest, IgnoreKeyboardBoundsChange) {
  InitKeyboardBounds();

  std::unique_ptr<WindowOwner> window_owner(
      CreateTestWindow(keyboard_bounds()));
  WmWindow* window = window_owner->window();
  // The additional SetBounds() is needed as the aura-mus case uses Widget,
  // which alters the supplied bounds.
  window->SetBounds(keyboard_bounds());
  window->GetWindowState()->set_ignore_keyboard_bounds_change(true);
  window->Activate();

  EXPECT_EQ(keyboard_bounds(), window->GetBounds());
  ShowKeyboard();
  EXPECT_EQ(keyboard_bounds(), window->GetBounds());
}

}  // namespace ash
