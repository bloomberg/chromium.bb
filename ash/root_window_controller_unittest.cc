// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::Window;
using views::Widget;

namespace ash {
namespace {

class TestDelegate : public views::WidgetDelegateView {
 public:
  explicit TestDelegate(bool system_modal) : system_modal_(system_modal) {}
  virtual ~TestDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  virtual ui::ModalType GetModalType() const OVERRIDE {
    return system_modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_NONE;
  }

 private:
  bool system_modal_;
  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class DeleteOnBlurDelegate : public aura::test::TestWindowDelegate,
                             public aura::client::FocusChangeObserver {
 public:
  DeleteOnBlurDelegate() : window_(NULL) {}
  virtual ~DeleteOnBlurDelegate() {}

  void SetWindow(aura::Window* window) {
    window_ = window;
    aura::client::SetFocusChangeObserver(window_, this);
  }

 private:
  // aura::test::TestWindowDelegate overrides:
  virtual bool CanFocus() OVERRIDE {
    return true;
  }

  // aura::client::FocusChangeObserver implementation:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) OVERRIDE {
    if (window_ == lost_focus)
      delete window_;
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnBlurDelegate);
};

}  // namespace

namespace test {

class RootWindowControllerTest : public test::AshTestBase {
 public:
  views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
    views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
        NULL, CurrentContext(), bounds);
    widget->Show();
    return widget;
  }

  views::Widget* CreateModalWidget(const gfx::Rect& bounds) {
    views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
        new TestDelegate(true), CurrentContext(), bounds);
    widget->Show();
    return widget;
  }

  views::Widget* CreateModalWidgetWithParent(const gfx::Rect& bounds,
                                             gfx::NativeWindow parent) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParentAndBounds(new TestDelegate(true),
                                                       parent,
                                                       bounds);
    widget->Show();
    return widget;
  }

  aura::Window* GetModalContainer(aura::RootWindow* root_window) {
    return Shell::GetContainer(
        root_window,
        ash::internal::kShellWindowId_SystemModalContainer);
  }
};

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_MoveWindows_Basic DISABLED_MoveWindows_Basic
#else
#define MAYBE_MoveWindows_Basic MoveWindows_Basic
#endif

TEST_F(RootWindowControllerTest, MAYBE_MoveWindows_Basic) {
  UpdateDisplay("600x600,500x500");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  internal::RootWindowController* controller =
      Shell::GetPrimaryRootWindowController();
  controller->SetShelfAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  views::Widget* normal = CreateTestWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("650,10 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("50,10 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* maximized = CreateTestWidget(gfx::Rect(700, 10, 100, 100));
  maximized->Maximize();
  EXPECT_EQ(root_windows[1], maximized->GetNativeView()->GetRootWindow());
  if (Shell::IsLauncherPerDisplayEnabled()) {
    EXPECT_EQ("600,0 500x452", maximized->GetWindowBoundsInScreen().ToString());
    EXPECT_EQ("0,0 500x452",
              maximized->GetNativeView()->GetBoundsInRootWindow().ToString());
  } else {
    EXPECT_EQ("600,0 500x500", maximized->GetWindowBoundsInScreen().ToString());
    EXPECT_EQ("0,0 500x500",
              maximized->GetNativeView()->GetBoundsInRootWindow().ToString());
  }

  views::Widget* minimized = CreateTestWidget(gfx::Rect(800, 10, 100, 100));
  minimized->Minimize();
  EXPECT_EQ(root_windows[1], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("800,10 100x100",
            minimized->GetWindowBoundsInScreen().ToString());

  views::Widget* fullscreen = CreateTestWidget(gfx::Rect(900, 10, 100, 100));
  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[1], fullscreen->GetNativeView()->GetRootWindow());

  EXPECT_EQ("600,0 500x500",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 500x500",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Make sure a window that will delete itself when losing focus
  // will not crash.
  aura::WindowTracker tracker;
  DeleteOnBlurDelegate delete_on_blur_delegate;
  aura::Window* d2 = CreateTestWindowInShellWithDelegate(
      &delete_on_blur_delegate, 0, gfx::Rect(50, 50, 100, 100));
  delete_on_blur_delegate.SetWindow(d2);
  aura::client::GetFocusClient(root_windows[0])->FocusWindow(d2);
  tracker.Add(d2);

  UpdateDisplay("600x600");

  // d2 must have been deleted.
  EXPECT_FALSE(tracker.Contains(d2));

  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("50,10 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("50,10 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Maximized area on primary display has 3px (given as
  // kAutoHideSize in shelf_layout_manager.cc) inset at the bottom.
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("0,0 600x597",
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x597",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  EXPECT_EQ(root_windows[0], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("200,10 100x100",
            minimized->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], fullscreen->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(fullscreen->IsFullscreen());
  EXPECT_EQ("0,0 600x600",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x600",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Test if the restore bounds are correctly updated.
  wm::RestoreWindow(maximized->GetNativeView());
  EXPECT_EQ("100,10 100x100", maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("100,10 100x100",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  fullscreen->SetFullscreen(false);
  EXPECT_EQ("300,10 100x100",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("300,10 100x100",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());
}

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_MoveWindows_Modal DISABLED_MoveWindows_Modal
#else
#define MAYBE_MoveWindows_Modal MoveWindows_Modal
#endif

TEST_F(RootWindowControllerTest, MAYBE_MoveWindows_Modal) {
  UpdateDisplay("500x500,500x500");

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  // Emulate virtual screen coordinate system.
  root_windows[0]->SetBounds(gfx::Rect(0, 0, 500, 500));
  root_windows[1]->SetBounds(gfx::Rect(500, 0, 500, 500));

  views::Widget* normal = CreateTestWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(normal->GetNativeView()));

  views::Widget* modal = CreateModalWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(GetModalContainer(root_windows[1])->Contains(
      modal->GetNativeView()));
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  aura::test::EventGenerator generator_1st(root_windows[0]);
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  UpdateDisplay("500x500");
  EXPECT_EQ(root_windows[0], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
}

TEST_F(RootWindowControllerTest, ModalContainer) {
  UpdateDisplay("600x600");
  Shell* shell = Shell::GetInstance();
  internal::RootWindowController* controller =
      shell->GetPrimaryRootWindowController();
  EXPECT_EQ(user::LOGGED_IN_USER,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  views::Widget* session_modal_widget =
      CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              session_modal_widget->GetNativeView()));

  shell->delegate()->LockScreen();
  EXPECT_EQ(user::LOGGED_IN_LOCKED,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  aura::Window* lock_container =
      Shell::GetContainer(controller->root_window(),
                          internal::kShellWindowId_LockScreenContainer);
  views::Widget* lock_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              lock_modal_widget->GetNativeView()));
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
        internal::kShellWindowId_SystemModalContainer)->layout_manager(),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeView()));

  shell->delegate()->UnlockScreen();
}

TEST_F(RootWindowControllerTest, ModalContainerNotLoggedInLoggedIn) {
  UpdateDisplay("600x600");
  Shell* shell = Shell::GetInstance();

  // Configure login screen environment.
  SetUserLoggedIn(false);
  EXPECT_EQ(user::LOGGED_IN_NONE,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_FALSE(shell->delegate()->IsUserLoggedIn());
  EXPECT_FALSE(shell->delegate()->IsSessionStarted());

  internal::RootWindowController* controller =
      shell->GetPrimaryRootWindowController();
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  aura::Window* lock_container =
      Shell::GetContainer(controller->root_window(),
                          internal::kShellWindowId_LockScreenContainer);
  views::Widget* login_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_LockSystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              login_modal_widget->GetNativeView()));
  login_modal_widget->Close();

  // Configure user session environment.
  SetUserLoggedIn(true);
  SetSessionStarted(true);
  EXPECT_EQ(user::LOGGED_IN_USER,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_TRUE(shell->delegate()->IsUserLoggedIn());
  EXPECT_TRUE(shell->delegate()->IsSessionStarted());
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(NULL));

  views::Widget* session_modal_widget =
        CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(Shell::GetContainer(controller->root_window(),
      internal::kShellWindowId_SystemModalContainer)->layout_manager(),
          controller->GetSystemModalLayoutManager(
              session_modal_widget->GetNativeView()));
}

// Ensure a workspace with two windows reports immersive mode even if only
// one has the property set.
TEST_F(RootWindowControllerTest, ImmersiveMode) {
  UpdateDisplay("600x600");
  internal::RootWindowController* controller =
      Shell::GetInstance()->GetPrimaryRootWindowController();

  // Open a maximized window.
  Widget* w1 = CreateTestWidget(gfx::Rect(0, 1, 250, 251));
  w1->Maximize();

  // Immersive mode off by default.
  EXPECT_FALSE(controller->IsImmersiveMode());

  // Enter immersive mode.
  w1->GetNativeWindow()->SetProperty(ash::internal::kImmersiveModeKey, true);
  EXPECT_TRUE(controller->IsImmersiveMode());

  // Add a child, like a print window.  Still in immersive mode.
  Widget* w2 =
      Widget::CreateWindowWithParentAndBounds(NULL,
                                              w1->GetNativeWindow(),
                                              gfx::Rect(0, 1, 150, 151));
  w2->Show();
  EXPECT_TRUE(controller->IsImmersiveMode());
}

}  // namespace test
}  // namespace ash
