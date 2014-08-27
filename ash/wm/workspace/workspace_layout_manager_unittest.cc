// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <string>

#include "ash/display/display_layout.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/maximize_mode/workspace_backdrop_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

class MaximizeDelegateView : public views::WidgetDelegateView {
 public:
  explicit MaximizeDelegateView(const gfx::Rect& initial_bounds)
      : initial_bounds_(initial_bounds) {
  }
  virtual ~MaximizeDelegateView() {}

  virtual bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE {
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
  TestShellObserver() : call_count_(0),
                        is_fullscreen_(false) {
    Shell::GetInstance()->AddShellObserver(this);
  }

  virtual ~TestShellObserver() {
    Shell::GetInstance()->RemoveShellObserver(this);
  }

  virtual void OnFullscreenStateChanged(bool is_fullscreen,
                                        aura::Window* root_window) OVERRIDE {
    call_count_++;
    is_fullscreen_ = is_fullscreen;
  }

  int call_count() const {
    return call_count_;
  }

  bool is_fullscreen() const {
    return is_fullscreen_;
  }

 private:
  int call_count_;
  bool is_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(TestShellObserver);
};

}  // namespace

typedef test::AshTestBase WorkspaceLayoutManagerTest;

// Verifies that a window containing a restore coordinate will be restored to
// to the size prior to minimize, keeping the restore rectangle in tact (if
// there is one).
TEST_F(WorkspaceLayoutManagerTest, RestoreFromMinimizeKeepsRestore) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // This will not be used for un-minimizing window.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(0, 0, 100, 100));
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ("0,0 100x100", window_state->GetRestoreBoundsInScreen().ToString());
  EXPECT_EQ("10,15 25x35", window.get()->bounds().ToString());

  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x300,500x400");
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenUtil::GetSecondaryDisplay());
  EXPECT_EQ(Shell::GetAllRootWindows()[1], window->GetRootWindow());
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
  EXPECT_EQ(Shell::GetPrimaryRootWindow(), window->GetRootWindow());
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
}

TEST_F(WorkspaceLayoutManagerTest, KeepMinimumVisibilityInDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x400,400x500");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  DisplayLayout layout(DisplayLayout::TOP, 0);
  Shell::GetInstance()->display_manager()->
      SetLayoutForCurrentDisplays(layout);
  EXPECT_EQ("0,-500 400x500", root_windows[1]->GetBoundsInScreen().ToString());

  scoped_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, -400, 200, 200)));
  EXPECT_EQ("10,-400 200x200", window1->GetBoundsInScreen().ToString());

  // Make sure the caption is visible.
  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, -600, 200, 200)));
  EXPECT_EQ("10,-500 200x200", window2->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, NoMinimumVisibilityForPopupWindows) {
  UpdateDisplay("300x400");

  // Create a popup window out of display boundaries and make sure it is not
  // moved to have minimum visibility.
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegateAndType(NULL,
                                                 ui::wm::WINDOW_TYPE_POPUP,
                                                 0,
                                                 gfx::Rect(400, 100, 50, 50)));
  EXPECT_EQ("400,100 50x50", window->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, KeepRestoredWindowInDisplay) {
  if (!SupportsHostWindowResize())
    return;
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Maximized -> Normal transition.
  window_state->Maximize();
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 30, 40));
  window_state->Restore();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-20,0 30x40", window->bounds().ToString());

  // Minimized -> Normal transition.
  window->SetBounds(gfx::Rect(-100, -100, 30, 40));
  window_state->Minimize();
  EXPECT_FALSE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  EXPECT_EQ("-100,-100 30x40", window->bounds().ToString());
  window->Show();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-20,0 30x40", window->bounds().ToString());

  // Fullscreen -> Normal transition.
  window->SetBounds(gfx::Rect(0, 0, 30, 40));  // reset bounds.
  ASSERT_EQ("0,0 30x40", window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(window->bounds(), window->GetRootWindow()->bounds());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 30, 40));
  window_state->Restore();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_EQ("-20,0 30x40", window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, MaximizeInDisplayToBeRestored) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("300x400,400x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x453", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 30x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(280, 0, 30, 40));
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x453", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("280,0 30x40", window->GetBoundsInScreen().ToString());

  // Restoring widget state.
  scoped_ptr<views::Widget> w1(new views::Widget);
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = new MaximizeDelegateView(gfx::Rect(400, 0, 30, 40));
  params.context = root_windows[0];
  w1->Init(params);
  w1->Show();
  EXPECT_TRUE(w1->IsMaximized());
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("300,0 400x453", w1->GetWindowBoundsInScreen().ToString());
  w1->Restore();
  EXPECT_EQ(root_windows[1], w1->GetNativeView()->GetRootWindow());
  EXPECT_EQ("400,0 30x40", w1->GetWindowBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, FullscreenInDisplayToBeRestored) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("300x400,400x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 30x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(280, 0, 30, 40));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("280,0 30x40", window->GetBoundsInScreen().ToString());
}

// WindowObserver implementation used by DontClobberRestoreBoundsWindowObserver.
// This code mirrors what BrowserFrameAsh does. In particular when this code
// sees the window was maximized it changes the bounds of a secondary
// window. The secondary window mirrors the status window.
class DontClobberRestoreBoundsWindowObserver : public aura::WindowObserver {
 public:
  DontClobberRestoreBoundsWindowObserver() : window_(NULL) {}

  void set_window(aura::Window* window) { window_ = window; }

  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    if (!window_)
      return;

    if (wm::GetWindowState(window)->IsMaximized()) {
      aura::Window* w = window_;
      window_ = NULL;

      gfx::Rect shelf_bounds(Shell::GetPrimaryRootWindowController()->
                             GetShelfLayoutManager()->GetIdealBounds());
      const gfx::Rect& window_bounds(w->bounds());
      w->SetBounds(gfx::Rect(window_bounds.x(), shelf_bounds.y() - 1,
                             window_bounds.width(), window_bounds.height()));
    }
  }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DontClobberRestoreBoundsWindowObserver);
};

// Creates a window, maximized the window and from within the maximized
// notification sets the bounds of a window to overlap the shelf. Verifies this
// doesn't effect the restore bounds.
TEST_F(WorkspaceLayoutManagerTest, DontClobberRestoreBounds) {
  DontClobberRestoreBoundsWindowObserver window_observer;
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(10, 20, 30, 40));
  // NOTE: for this test to exercise the failure the observer needs to be added
  // before the parent set. This mimics what BrowserFrameAsh does.
  window->AddObserver(&window_observer);
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->Activate();

  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(12, 20, 30, 40)));
  ::wm::AddTransientChild(window.get(), window2.get());
  window2->Show();

  window_observer.set_window(window2.get());
  window_state->Maximize();
  EXPECT_EQ("10,20 30x40",
            window_state->GetRestoreBoundsInScreen().ToString());
  window->RemoveObserver(&window_observer);
}

// Verifies when a window is maximized all descendant windows have a size.
TEST_F(WorkspaceLayoutManagerTest, ChildBoundsResetOnMaximize) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 30, 40)));
  window->Show();
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->Activate();
  scoped_ptr<aura::Window> child_window(
      aura::test::CreateTestWindowWithBounds(gfx::Rect(5, 6, 7, 8),
                                             window.get()));
  child_window->Show();
  window_state->Maximize();
  EXPECT_EQ("5,6 7x8", child_window->bounds().ToString());
}

// Verifies a window created with maximized state has the maximized
// bounds.
TEST_F(WorkspaceLayoutManagerTest, MaximizeWithEmptySize) {
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithBounds(gfx::Rect(0, 0, 0, 0),
                                             NULL));
  wm::GetWindowState(window.get())->Maximize();
  aura::Window* default_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_DefaultContainer);
  default_container->AddChild(window.get());
  window->Show();
  gfx::Rect work_area(
      Shell::GetScreen()->GetPrimaryDisplay().work_area());
  EXPECT_EQ(work_area.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, WindowShouldBeOnScreenWhenAdded) {
  // Normal window bounds shouldn't be changed.
  gfx::Rect window_bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds, window->bounds());

  // If the window is out of the workspace, it would be moved on screen.
  gfx::Rect root_window_bounds =
      Shell::GetInstance()->GetPrimaryRootWindow()->bounds();
  window_bounds.Offset(root_window_bounds.width(), root_window_bounds.height());
  ASSERT_FALSE(window_bounds.Intersects(root_window_bounds));
  scoped_ptr<aura::Window> out_window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds.size(), out_window->bounds().size());
  gfx::Rect bounds = out_window->bounds();
  bounds.Intersect(root_window_bounds);

  // 30% of the window edge must be visible.
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  aura::Window* parent = out_window->parent();
  parent->RemoveChild(out_window.get());
  out_window->SetBounds(gfx::Rect(-200, -200, 200, 200));
  // UserHasChangedWindowPositionOrSize flag shouldn't turn off this behavior.
  wm::GetWindowState(window.get())->set_bounds_changed_by_user(true);
  parent->AddChild(out_window.get());
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  // Make sure we always make more than 1/3 of the window edge visible even
  // if the initial bounds intersects with display.
  window_bounds.SetRect(-150, -150, 200, 200);
  bounds = window_bounds;
  bounds.Intersect(root_window_bounds);

  // Make sure that the initial bounds' visible area is less than 26%
  // so that the auto adjustment logic kicks in.
  ASSERT_LT(bounds.width(), out_window->bounds().width() * 0.26);
  ASSERT_LT(bounds.height(), out_window->bounds().height() * 0.26);
  ASSERT_TRUE(window_bounds.Intersects(root_window_bounds));

  scoped_ptr<aura::Window> partially_out_window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(window_bounds.size(), partially_out_window->bounds().size());
  bounds = partially_out_window->bounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  // Make sure the window whose 30% width/height is bigger than display
  // will be placed correctly.
  window_bounds.SetRect(-1900, -1900, 3000, 3000);
  scoped_ptr<aura::Window> window_bigger_than_display(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_GE(root_window_bounds.width(),
            window_bigger_than_display->bounds().width());
  EXPECT_GE(root_window_bounds.height(),
            window_bigger_than_display->bounds().height());

  bounds = window_bigger_than_display->bounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);
}

// Verifies the size of a window is enforced to be smaller than the work area.
TEST_F(WorkspaceLayoutManagerTest, SizeToWorkArea) {
  // Normal window bounds shouldn't be changed.
  gfx::Size work_area(
      Shell::GetScreen()->GetPrimaryDisplay().work_area().size());
  const gfx::Rect window_bounds(
      100, 101, work_area.width() + 1, work_area.height() + 2);
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
      window->bounds().ToString());

  // Directly setting the bounds triggers a slightly different code path. Verify
  // that too.
  window->SetBounds(window_bounds);
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
      window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, NotifyFullscreenChanges) {
  TestShellObserver observer;
  scoped_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  scoped_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 30, 40)));
  wm::WindowState* window_state1 = wm::GetWindowState(window1.get());
  wm::WindowState* window_state2 = wm::GetWindowState(window2.get());
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
  window2.reset();
  EXPECT_EQ(6, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());
}

// Following "Solo" tests were originally written for BaseLayoutManager.
namespace {

class WorkspaceLayoutManagerSoloTest : public test::AshTestBase {
 public:
  WorkspaceLayoutManagerSoloTest() {}
  virtual ~WorkspaceLayoutManagerSoloTest() {}

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithBounds(bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerSoloTest);
};

}  // namespace

// Tests normal->maximize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Maximize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  // Maximized window fills the work area, not the whole display.
  EXPECT_EQ(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Tests normal->minimize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Minimize) {
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
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {
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
TEST_F(WorkspaceLayoutManagerSoloTest, FocusDuringUnminimize) {
  FocusDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(100, 100, 100, 100)));
  delegate.set_window(window.get());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_MINIMIZED, delegate.GetShowStateAndReset());
  window->Show();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, delegate.GetShowStateAndReset());
}

// Tests maximized window size during root window resize.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  gfx::Rect initial_work_area_bounds =
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get());
  EXPECT_EQ(initial_work_area_bounds.ToString(), window->bounds().ToString());
  // Enlarge the root window.  We should still match the work area size.
  UpdateDisplay("900x700");
  EXPECT_EQ(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  EXPECT_NE(
      initial_work_area_bounds.ToString(),
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString());
}

// Tests normal->fullscreen->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Fullscreen) {
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
TEST_F(WorkspaceLayoutManagerSoloTest, FullscreenRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  // Fullscreen window fills the whole display.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
  // Enlarge the root window.  We should still match the display size.
  UpdateDisplay("800x600");
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window.get()).bounds().ToString(),
            window->bounds().ToString());
}

// Tests that when the screen gets smaller the windows aren't bigger than
// the screen.
TEST_F(WorkspaceLayoutManagerSoloTest, RootWindowResizeShrinksWindows) {
  scoped_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 500, 400)));
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  // Invariant: Window is smaller than work area.
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window narrower than our window.
  UpdateDisplay("300x400");
  work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window shorter than our window.
  UpdateDisplay("300x200");
  work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Enlarging the root window does not change the window bounds.
  gfx::Rect old_bounds = window->bounds();
  UpdateDisplay("800x600");
  EXPECT_EQ(old_bounds.width(), window->bounds().width());
  EXPECT_EQ(old_bounds.height(), window->bounds().height());
}

// Verifies maximizing sets the restore bounds, and restoring
// restores the bounds.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeSetsRestoreBounds) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(10, 20, 30, 40)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("10,20 30x40", window_state->GetRestoreBoundsInParent().ToString());

  // Restore it, which should restore bounds and reset restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ("10,20 30x40", window->bounds().ToString());
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies maximizing keeps the restore bounds if set.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeResetsRestoreBounds) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInParent(gfx::Rect(10, 11, 12, 13));

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ("10,11 12x13", window_state->GetRestoreBoundsInParent().ToString());
}

// Verifies that the restore bounds do not get reset when restoring to a
// maximzied state from a minimized state.
TEST_F(WorkspaceLayoutManagerSoloTest,
       BoundsAfterRestoringToMaximizeFromMinimize) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  wm::WindowState* window_state = wm::GetWindowState(window.get());
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
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Verify if the window is not resized during screen lock. See: crbug.com/173127
TEST_F(WorkspaceLayoutManagerSoloTest, NotResizeWhenScreenIsLocked) {
  SetCanLockScreen(true);
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  // window with AlwaysOnTop will be managed by BaseLayoutManager.
  window->SetProperty(aura::client::kAlwaysOnTopKey, true);
  window->Show();

  ShelfLayoutManager* shelf = ShelfLayoutManager::ForShelf(window.get());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  window->SetBounds(ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()));
  gfx::Rect window_bounds = window->bounds();
  EXPECT_EQ(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window_bounds.ToString());

  Shell::GetInstance()->session_state_delegate()->LockScreen();
  shelf->UpdateVisibilityState();
  EXPECT_NE(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window_bounds.ToString());

  Shell::GetInstance()->session_state_delegate()->UnlockScreen();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(window_bounds.ToString(), window->bounds().ToString());
}

// Following tests are written to test the backdrop functionality.

namespace {

class WorkspaceLayoutManagerBackdropTest : public test::AshTestBase {
 public:
  WorkspaceLayoutManagerBackdropTest() {}
  virtual ~WorkspaceLayoutManagerBackdropTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    UpdateDisplay("800x600");
    default_container_ = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                             kShellWindowId_DefaultContainer);
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithBounds(bounds);
    return window;
  }

  // Turn the top window back drop on / off.
  void ShowTopWindowBackdrop(bool show) {
    scoped_ptr<ash::WorkspaceLayoutManagerDelegate> backdrop;
    if (show) {
      backdrop.reset(new ash::WorkspaceBackdropDelegate(default_container_));
    }
    (static_cast<WorkspaceLayoutManager*>(default_container_->layout_manager()))
        ->SetMaximizeBackdropDelegate(backdrop.Pass());
    // Closing and / or opening can be a delayed operation.
    base::MessageLoop::current()->RunUntilIdle();
  }

  // Return the default container.
  aura::Window* default_container() { return default_container_; }

  // Return the order of windows (top most first) as they are in the default
  // container. If the window is visible it will be a big letter, otherwise a
  // small one. The backdrop will be an X and unknown windows will be shown as
  // '!'.
  std::string GetWindowOrderAsString(aura::Window* backdrop,
                                     aura::Window* wa,
                                     aura::Window* wb,
                                     aura::Window* wc) {
    std::string result;
    for (int i = static_cast<int>(default_container()->children().size()) - 1;
         i >= 0;
         --i) {
      if (!result.empty())
        result += ",";
      if (default_container()->children()[i] == wa)
        result += default_container()->children()[i]->IsVisible() ? "A" : "a";
      else if (default_container()->children()[i] == wb)
        result += default_container()->children()[i]->IsVisible() ? "B" : "b";
      else if (default_container()->children()[i] == wc)
        result += default_container()->children()[i]->IsVisible() ? "C" : "c";
      else if (default_container()->children()[i] == backdrop)
        result += default_container()->children()[i]->IsVisible() ? "X" : "x";
      else
        result += "!";
    }
    return result;
  }

 private:
  // The default container.
  aura::Window* default_container_;

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
  ASSERT_EQ(1U, default_container()->children().size());
  EXPECT_FALSE(default_container()->children()[0]->IsVisible());

  {
    // Add a window and make sure that the backdrop is the second child.
    scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
    window->Show();
    ASSERT_EQ(2U, default_container()->children().size());
    EXPECT_TRUE(default_container()->children()[0]->IsVisible());
    EXPECT_TRUE(default_container()->children()[1]->IsVisible());
    EXPECT_EQ(window.get(), default_container()->children()[1]);
    EXPECT_EQ(default_container()->bounds().ToString(),
              default_container()->children()[0]->bounds().ToString());
  }

  // With the window gone the backdrop should be invisible again.
  ASSERT_EQ(1U, default_container()->children().size());
  EXPECT_FALSE(default_container()->children()[0]->IsVisible());

  // Destroying the Backdrop should empty the container.
  ShowTopWindowBackdrop(false);
  ASSERT_EQ(0U, default_container()->children().size());
}

// Verify that the backdrop gets properly created and placed.
TEST_F(WorkspaceLayoutManagerBackdropTest, VerifyBackdropAndItsStacking) {
  scoped_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  window1->Show();

  // Get the default container and check that only a single window is in there.
  ASSERT_EQ(1U, default_container()->children().size());
  EXPECT_EQ(window1.get(), default_container()->children()[0]);
  EXPECT_EQ("A", GetWindowOrderAsString(NULL, window1.get(), NULL, NULL));

  // Create 2 more windows and check that they are also in the container.
  scoped_ptr<aura::Window> window2(CreateTestWindow(gfx::Rect(10, 2, 3, 4)));
  scoped_ptr<aura::Window> window3(CreateTestWindow(gfx::Rect(20, 2, 3, 4)));
  window2->Show();
  window3->Show();

  aura::Window* backdrop = NULL;
  EXPECT_EQ("C,B,A",
            GetWindowOrderAsString(backdrop, window1.get(), window2.get(),
                                   window3.get()));

  // Turn on the backdrop mode and check that the window shows up where it
  // should be (second highest number).
  ShowTopWindowBackdrop(true);
  backdrop = default_container()->children()[2];
  EXPECT_EQ("C,X,B,A",
            GetWindowOrderAsString(backdrop, window1.get(), window2.get(),
                                   window3.get()));

  // Switch the order of windows and check that it still remains in that
  // location.
  default_container()->StackChildAtTop(window2.get());
  EXPECT_EQ("B,X,C,A",
            GetWindowOrderAsString(backdrop, window1.get(), window2.get(),
                                   window3.get()));

  // Make the top window invisible and check.
  window2.get()->Hide();
  EXPECT_EQ("b,C,X,A",
            GetWindowOrderAsString(backdrop, window1.get(), window2.get(),
                                   window3.get()));
  // Then delete window after window and see that everything is in order.
  window1.reset();
  EXPECT_EQ("b,C,X",
            GetWindowOrderAsString(backdrop, window1.get(), window2.get(),
                                   window3.get()));
  window3.reset();
  EXPECT_EQ("b,x",
            GetWindowOrderAsString(backdrop, window1.get(), window2.get(),
                                   window3.get()));
  ShowTopWindowBackdrop(false);
  EXPECT_EQ("b",
            GetWindowOrderAsString(NULL, window1.get(), window2.get(),
                                   window3.get()));
}

// Tests that when hidding the shelf, that the backdrop resizes to fill the
// entire workspace area.
TEST_F(WorkspaceLayoutManagerBackdropTest, ShelfVisibilityChangesBounds) {
  ShelfLayoutManager* shelf_layout_manager =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  ShowTopWindowBackdrop(true);
  RunAllPendingInMessageLoop();

  ASSERT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());
  gfx::Rect initial_bounds = default_container()->children()[0]->bounds();
  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  shelf_layout_manager->UpdateVisibilityState();

  // When the shelf is re-shown WorkspaceLayoutManager shrinks all children
  // including the backdrop.
  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf_layout_manager->UpdateVisibilityState();
  gfx::Rect reduced_bounds = default_container()->children()[0]->bounds();
  EXPECT_LT(reduced_bounds.height(), initial_bounds.height());

  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  shelf_layout_manager->UpdateVisibilityState();

  EXPECT_GT(default_container()->children()[0]->bounds().height(),
            reduced_bounds.height());
}

class WorkspaceLayoutManagerKeyboardTest : public test::AshTestBase {
 public:
  WorkspaceLayoutManagerKeyboardTest() {}
  virtual ~WorkspaceLayoutManagerKeyboardTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    UpdateDisplay("800x600");
    aura::Window* default_container = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(), kShellWindowId_DefaultContainer);
    layout_manager_ = static_cast<WorkspaceLayoutManager*>(
        default_container->layout_manager());
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithBounds(bounds);
  }

  void ShowKeyboard() {
    layout_manager_->OnKeyboardBoundsChanging(keyboard_bounds_);
    restore_work_area_insets_ = Shell::GetScreen()->GetPrimaryDisplay().
        GetWorkAreaInsets();
    Shell::GetInstance()->SetDisplayWorkAreaInsets(
        Shell::GetPrimaryRootWindow(),
        gfx::Insets(0, 0, keyboard_bounds_.height(), 0));
  }

  void HideKeyboard() {
    Shell::GetInstance()->SetDisplayWorkAreaInsets(
        Shell::GetPrimaryRootWindow(),
        restore_work_area_insets_);
    layout_manager_->OnKeyboardBoundsChanging(gfx::Rect());
  }

  void SetKeyboardBounds(const gfx::Rect& bounds) {
    keyboard_bounds_ = bounds;
  }

  void Focus(ui::TextInputClient* text_input_client) {
    if (switches::IsTextInputFocusManagerEnabled()) {
      ui::TextInputFocusManager::GetInstance()->FocusTextInputClient(
          text_input_client);
    } else {
      aura::Window* root_window =
          ash::Shell::GetInstance()->GetPrimaryRootWindow();
      ui::InputMethod* input_method =
          root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
      input_method->SetFocusedTextInputClient(text_input_client);
    }
  }

  void Blur(ui::TextInputClient* text_input_client) {
    if (switches::IsTextInputFocusManagerEnabled()) {
      ui::TextInputFocusManager::GetInstance()->BlurTextInputClient(
          text_input_client);
    } else {
      aura::Window* root_window =
          ash::Shell::GetInstance()->GetPrimaryRootWindow();
      ui::InputMethod* input_method =
          root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
      input_method->SetFocusedTextInputClient(NULL);
    }
  }

 private:
  gfx::Insets restore_work_area_insets_;
  gfx::Rect keyboard_bounds_;
  WorkspaceLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerKeyboardTest);
};

class FakeTextInputClient : public ui::DummyTextInputClient {
 public:
  explicit FakeTextInputClient(gfx::NativeWindow window) : window_(window) {}
  virtual ~FakeTextInputClient() {}

  virtual gfx::NativeWindow GetAttachedWindow() const OVERRIDE {
    return window_;
  }

 private:
  gfx::NativeWindow window_;

  DISALLOW_COPY_AND_ASSIGN(FakeTextInputClient);
};

// Tests that when a child window gains focus the top level window containing it
// is resized to fit the remaining workspace area.
TEST_F(WorkspaceLayoutManagerKeyboardTest, ChildWindowFocused) {
  gfx::Rect work_area(
      Shell::GetScreen()->GetPrimaryDisplay().work_area());
  gfx::Rect keyboard_bounds(work_area.x(),
                            work_area.y() + work_area.height() / 2,
                            work_area.width(),
                            work_area.height() / 2);

  SetKeyboardBounds(keyboard_bounds);

  aura::test::TestWindowDelegate delegate1;
  scoped_ptr<aura::Window> parent_window(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, work_area));
  aura::test::TestWindowDelegate delegate2;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate2, -1, work_area));
  parent_window->AddChild(window.get());

  FakeTextInputClient text_input_client(window.get());
  Focus(&text_input_client);

  int available_height =
      Shell::GetScreen()->GetPrimaryDisplay().bounds().height() -
      keyboard_bounds.height();

  gfx::Rect initial_window_bounds(50, 50, 100, 500);
  parent_window->SetBounds(initial_window_bounds);
  EXPECT_EQ(initial_window_bounds.ToString(),
            parent_window->bounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(50, 0, 100, available_height).ToString(),
            parent_window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(initial_window_bounds.ToString(),
            parent_window->bounds().ToString());

  Blur(&text_input_client);
}

TEST_F(WorkspaceLayoutManagerKeyboardTest, AdjustWindowForA11yKeyboard) {
  gfx::Rect work_area(
      Shell::GetScreen()->GetPrimaryDisplay().work_area());
  gfx::Rect keyboard_bounds(work_area.x(),
                            work_area.y() + work_area.height() / 2,
                            work_area.width(),
                            work_area.height() / 2);

  SetKeyboardBounds(keyboard_bounds);

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, work_area));

  FakeTextInputClient text_input_client(window.get());
  Focus(&text_input_client);

  int available_height =
      Shell::GetScreen()->GetPrimaryDisplay().bounds().height() -
      keyboard_bounds.height();

  EXPECT_EQ(gfx::Rect(work_area).ToString(), window->bounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(work_area.origin(),
            gfx::Size(work_area.width(), available_height)).ToString(),
            window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(gfx::Rect(work_area).ToString(), window->bounds().ToString());

  gfx::Rect small_window_bound(50, 50, 100, 500);
  window->SetBounds(small_window_bound);
  EXPECT_EQ(small_window_bound.ToString(), window->bounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(50, 0, 100, available_height).ToString(),
            window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(small_window_bound.ToString(), window->bounds().ToString());

  gfx::Rect occluded_window_bounds(50,
      keyboard_bounds.y() + keyboard_bounds.height()/2, 50,
      keyboard_bounds.height()/2);
  window->SetBounds(occluded_window_bounds);
  EXPECT_EQ(occluded_window_bounds.ToString(),
      occluded_window_bounds.ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(50,
                      keyboard_bounds.y() - keyboard_bounds.height()/2,
                      occluded_window_bounds.width(),
                      occluded_window_bounds.height()).ToString(),
            window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(occluded_window_bounds.ToString(), window->bounds().ToString());

  Blur(&text_input_client);
}

}  // namespace ash
