// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
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

  aura::Window* GetModalContainer(aura::Window* root_window) {
    return Shell::GetContainer(root_window,
                               ash::kShellWindowId_SystemModalContainer);
  }
};

TEST_F(RootWindowControllerTest, MoveWindows_Basic) {
  if (!SupportsMultipleDisplays())
    return;
  // Windows origin should be doubled when moved to the 1st display.
  UpdateDisplay("600x600,300x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  ShelfLayoutManager* shelf_layout_manager =
      controller->GetShelfLayoutManager();
  shelf_layout_manager->SetAutoHideBehavior(
      ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  views::Widget* normal = CreateTestWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("650,10 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("50,10 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* maximized = CreateTestWidget(gfx::Rect(700, 10, 100, 100));
  maximized->Maximize();
  EXPECT_EQ(root_windows[1], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("600,0 300x253", maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 300x253",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* minimized = CreateTestWidget(gfx::Rect(800, 10, 100, 100));
  minimized->Minimize();
  EXPECT_EQ(root_windows[1], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("800,10 100x100",
            minimized->GetWindowBoundsInScreen().ToString());

  views::Widget* fullscreen = CreateTestWidget(gfx::Rect(850, 10, 100, 100));
  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[1], fullscreen->GetNativeView()->GetRootWindow());

  EXPECT_EQ("600,0 300x300",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 300x300",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* unparented_control = new Widget;
  Widget::InitParams params;
  params.bounds = gfx::Rect(650, 10, 100, 100);
  params.context = CurrentContext();
  params.type = Widget::InitParams::TYPE_CONTROL;
  unparented_control->Init(params);
  EXPECT_EQ(root_windows[1],
            unparented_control->GetNativeView()->GetRootWindow());
  EXPECT_EQ(kShellWindowId_UnparentedControlContainer,
            unparented_control->GetNativeView()->parent()->id());

  aura::Window* panel = CreateTestWindowInShellWithDelegateAndType(
      NULL, ui::wm::WINDOW_TYPE_PANEL, 0, gfx::Rect(700, 100, 100, 100));
  EXPECT_EQ(root_windows[1], panel->GetRootWindow());
  EXPECT_EQ(kShellWindowId_PanelContainer, panel->parent()->id());

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
  EXPECT_EQ("100,20 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("100,20 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Maximized area on primary display has 3px (given as
  // kAutoHideSize in shelf_layout_manager.cc) inset at the bottom.

  // First clear fullscreen status, since both fullscreen and maximized windows
  // share the same desktop workspace, which cancels the shelf status.
  fullscreen->SetFullscreen(false);
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("0,0 600x597",
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x597",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Set fullscreen to true. In that case the 3px inset becomes invisible so
  // the maximized window can also use the area fully.
  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("0,0 600x600",
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x600",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  EXPECT_EQ(root_windows[0], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("400,20 100x100",
            minimized->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], fullscreen->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(fullscreen->IsFullscreen());
  EXPECT_EQ("0,0 600x600",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x600",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Test if the restore bounds are correctly updated.
  wm::GetWindowState(maximized->GetNativeView())->Restore();
  EXPECT_EQ("200,20 100x100", maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("200,20 100x100",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  fullscreen->SetFullscreen(false);
  EXPECT_EQ("500,20 100x100",
            fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("500,20 100x100",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Test if the unparented widget has moved.
  EXPECT_EQ(root_windows[0],
            unparented_control->GetNativeView()->GetRootWindow());
  EXPECT_EQ(kShellWindowId_UnparentedControlContainer,
            unparented_control->GetNativeView()->parent()->id());

  // Test if the panel has moved.
  EXPECT_EQ(root_windows[0], panel->GetRootWindow());
  EXPECT_EQ(kShellWindowId_PanelContainer, panel->parent()->id());
}

TEST_F(RootWindowControllerTest, MoveWindows_Modal) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x500,500x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
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

  ui::test::EventGenerator generator_1st(root_windows[0]);
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
  RootWindowController* controller = shell->GetPrimaryRootWindowController();
  EXPECT_EQ(user::LOGGED_IN_USER,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(NULL));

  views::Widget* session_modal_widget =
      CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeView()));

  shell->session_state_delegate()->LockScreen();
  EXPECT_EQ(user::LOGGED_IN_LOCKED,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(controller->GetContainer(kShellWindowId_LockSystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(NULL));

  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  views::Widget* lock_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(controller->GetContainer(kShellWindowId_LockSystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(
                lock_modal_widget->GetNativeView()));
  EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeView()));

  shell->session_state_delegate()->UnlockScreen();
}

TEST_F(RootWindowControllerTest, ModalContainerNotLoggedInLoggedIn) {
  UpdateDisplay("600x600");
  Shell* shell = Shell::GetInstance();

  // Configure login screen environment.
  SetUserLoggedIn(false);
  EXPECT_EQ(user::LOGGED_IN_NONE,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(0, shell->session_state_delegate()->NumberOfLoggedInUsers());
  EXPECT_FALSE(shell->session_state_delegate()->IsActiveUserSessionStarted());

  RootWindowController* controller = shell->GetPrimaryRootWindowController();
  EXPECT_EQ(controller->GetContainer(kShellWindowId_LockSystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(NULL));

  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  views::Widget* login_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(controller->GetContainer(kShellWindowId_LockSystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(
                login_modal_widget->GetNativeView()));
  login_modal_widget->Close();

  // Configure user session environment.
  SetUserLoggedIn(true);
  SetSessionStarted(true);
  EXPECT_EQ(user::LOGGED_IN_USER,
            shell->system_tray_delegate()->GetUserLoginStatus());
  EXPECT_EQ(1, shell->session_state_delegate()->NumberOfLoggedInUsers());
  EXPECT_TRUE(shell->session_state_delegate()->IsActiveUserSessionStarted());
  EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(NULL));

  views::Widget* session_modal_widget =
        CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                ->layout_manager(),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeView()));
}

TEST_F(RootWindowControllerTest, ModalContainerBlockedSession) {
  UpdateDisplay("600x600");
  Shell* shell = Shell::GetInstance();
  RootWindowController* controller = shell->GetPrimaryRootWindowController();
  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS;
       ++block_reason) {
    views::Widget* session_modal_widget =
          CreateModalWidget(gfx::Rect(300, 10, 100, 100));
    EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                  ->layout_manager(),
              controller->GetSystemModalLayoutManager(
                  session_modal_widget->GetNativeView()));
    EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                  ->layout_manager(),
              controller->GetSystemModalLayoutManager(NULL));
    session_modal_widget->Close();

    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));

    EXPECT_EQ(controller->GetContainer(kShellWindowId_LockSystemModalContainer)
                  ->layout_manager(),
              controller->GetSystemModalLayoutManager(NULL));

    views::Widget* lock_modal_widget =
        CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100),
                                    lock_container);
    EXPECT_EQ(controller->GetContainer(kShellWindowId_LockSystemModalContainer)
                  ->layout_manager(),
              controller->GetSystemModalLayoutManager(
                  lock_modal_widget->GetNativeView()));

    session_modal_widget =
          CreateModalWidget(gfx::Rect(300, 10, 100, 100));
    EXPECT_EQ(controller->GetContainer(kShellWindowId_SystemModalContainer)
                  ->layout_manager(),
              controller->GetSystemModalLayoutManager(
                  session_modal_widget->GetNativeView()));
    session_modal_widget->Close();

    lock_modal_widget->Close();
    UnblockUserSession();
  }
}

TEST_F(RootWindowControllerTest, GetWindowForFullscreenMode) {
  UpdateDisplay("600x600");
  RootWindowController* controller =
      Shell::GetInstance()->GetPrimaryRootWindowController();

  Widget* w1 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w1->Maximize();
  Widget* w2 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w2->SetFullscreen(true);
  // |w3| is a transient child of |w2|.
  Widget* w3 = Widget::CreateWindowWithParentAndBounds(NULL,
      w2->GetNativeWindow(), gfx::Rect(0, 0, 100, 100));

  // Test that GetWindowForFullscreenMode() finds the fullscreen window when one
  // of its transient children is active.
  w3->Activate();
  EXPECT_EQ(w2->GetNativeWindow(), controller->GetWindowForFullscreenMode());

  // If the topmost window is not fullscreen, it returns NULL.
  w1->Activate();
  EXPECT_EQ(NULL, controller->GetWindowForFullscreenMode());
  w1->Close();
  w3->Close();

  // Only w2 remains, if minimized GetWindowForFullscreenMode should return
  // NULL.
  w2->Activate();
  EXPECT_EQ(w2->GetNativeWindow(), controller->GetWindowForFullscreenMode());
  w2->Minimize();
  EXPECT_EQ(NULL, controller->GetWindowForFullscreenMode());
}

TEST_F(RootWindowControllerTest, MultipleDisplaysGetWindowForFullscreenMode) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x600,600x600");
  Shell::RootWindowControllerList controllers =
      Shell::GetInstance()->GetAllRootWindowControllers();

  Widget* w1 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w1->Maximize();
  Widget* w2 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w2->SetFullscreen(true);
  Widget* w3 = CreateTestWidget(gfx::Rect(600, 0, 100, 100));

  EXPECT_EQ(w1->GetNativeWindow()->GetRootWindow(),
            controllers[0]->GetRootWindow());
  EXPECT_EQ(w2->GetNativeWindow()->GetRootWindow(),
            controllers[0]->GetRootWindow());
  EXPECT_EQ(w3->GetNativeWindow()->GetRootWindow(),
            controllers[1]->GetRootWindow());

  w1->Activate();
  EXPECT_EQ(NULL, controllers[0]->GetWindowForFullscreenMode());
  EXPECT_EQ(NULL, controllers[1]->GetWindowForFullscreenMode());

  w2->Activate();
  EXPECT_EQ(w2->GetNativeWindow(),
            controllers[0]->GetWindowForFullscreenMode());
  EXPECT_EQ(NULL, controllers[1]->GetWindowForFullscreenMode());

  // Verify that the first root window controller remains in fullscreen mode
  // when a window on the other display is activated.
  w3->Activate();
  EXPECT_EQ(w2->GetNativeWindow(),
            controllers[0]->GetWindowForFullscreenMode());
  EXPECT_EQ(NULL, controllers[1]->GetWindowForFullscreenMode());
}

// Test that user session window can't be focused if user session blocked by
// some overlapping UI.
TEST_F(RootWindowControllerTest, FocusBlockedWindow) {
  UpdateDisplay("600x600");
  RootWindowController* controller =
      Shell::GetInstance()->GetPrimaryRootWindowController();
  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  aura::Window* lock_window = Widget::CreateWindowWithParentAndBounds(NULL,
      lock_container, gfx::Rect(0, 0, 100, 100))->GetNativeView();
  lock_window->Show();
  aura::Window* session_window =
      CreateTestWidget(gfx::Rect(0, 0, 100, 100))->GetNativeView();
  session_window->Show();

  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS;
       ++block_reason) {
    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));
    lock_window->Focus();
    EXPECT_TRUE(lock_window->HasFocus());
    session_window->Focus();
    EXPECT_FALSE(session_window->HasFocus());
    UnblockUserSession();
  }
}

// Tracks whether OnWindowDestroying() has been invoked.
class DestroyedWindowObserver : public aura::WindowObserver {
 public:
  DestroyedWindowObserver() : destroyed_(false), window_(NULL) {}
  virtual ~DestroyedWindowObserver() {
    Shutdown();
  }

  void SetWindow(Window* window) {
    window_ = window;
    window->AddObserver(this);
  }

  bool destroyed() const { return destroyed_; }

  // WindowObserver overrides:
  virtual void OnWindowDestroying(Window* window) OVERRIDE {
    destroyed_ = true;
    Shutdown();
  }

 private:
  void Shutdown() {
    if (!window_)
      return;
    window_->RemoveObserver(this);
    window_ = NULL;
  }

  bool destroyed_;
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DestroyedWindowObserver);
};

// Verifies shutdown doesn't delete windows that are not owned by the parent.
TEST_F(RootWindowControllerTest, DontDeleteWindowsNotOwnedByParent) {
  DestroyedWindowObserver observer1;
  aura::test::TestWindowDelegate delegate1;
  aura::Window* window1 = new aura::Window(&delegate1);
  window1->SetType(ui::wm::WINDOW_TYPE_CONTROL);
  window1->set_owned_by_parent(false);
  observer1.SetWindow(window1);
  window1->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  aura::client::ParentWindowWithContext(
      window1, Shell::GetInstance()->GetPrimaryRootWindow(), gfx::Rect());

  DestroyedWindowObserver observer2;
  aura::Window* window2 = new aura::Window(NULL);
  window2->set_owned_by_parent(false);
  observer2.SetWindow(window2);
  window2->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  Shell::GetInstance()->GetPrimaryRootWindow()->AddChild(window2);

  Shell::GetInstance()->GetPrimaryRootWindowController()->CloseChildWindows();

  ASSERT_FALSE(observer1.destroyed());
  delete window1;

  ASSERT_FALSE(observer2.destroyed());
  delete window2;
}

typedef test::NoSessionAshTestBase NoSessionRootWindowControllerTest;

// Make sure that an event handler exists for entire display area.
TEST_F(NoSessionRootWindowControllerTest, Event) {
  // Hide the shelf since it might otherwise get an event target.
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  ShelfLayoutManager* shelf_layout_manager =
      controller->GetShelfLayoutManager();
  shelf_layout_manager->SetAutoHideBehavior(
      ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN);

  aura::Window* root = Shell::GetPrimaryRootWindow();
  const gfx::Size size = root->bounds().size();
  aura::Window* event_target = root->GetEventHandlerForPoint(gfx::Point(0, 0));
  EXPECT_TRUE(event_target);
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(gfx::Point(0, size.height() - 1)));
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(gfx::Point(size.width() - 1, 0)));
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(gfx::Point(0, size.height() - 1)));
  EXPECT_EQ(event_target,
            root->GetEventHandlerForPoint(
                gfx::Point(size.width() - 1, size.height() - 1)));
}

class VirtualKeyboardRootWindowControllerTest
    : public RootWindowControllerTest {
 public:
  VirtualKeyboardRootWindowControllerTest() {};
  virtual ~VirtualKeyboardRootWindowControllerTest() {};

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    test::AshTestBase::SetUp();
    Shell::GetPrimaryRootWindowController()->ActivateKeyboard(
        keyboard::KeyboardController::GetInstance());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardRootWindowControllerTest);
};

class MockTextInputClient : public ui::DummyTextInputClient {
 public:
  MockTextInputClient() :
      ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  virtual void EnsureCaretInRect(const gfx::Rect& rect) OVERRIDE {
    visible_rect_ = rect;
  }

  const gfx::Rect& visible_rect() const {
    return visible_rect_;
  }

 private:
  gfx::Rect visible_rect_;

  DISALLOW_COPY_AND_ASSIGN(MockTextInputClient);
};

class TargetHitTestEventHandler : public ui::test::TestEventHandler {
 public:
  TargetHitTestEventHandler() {}

  // ui::test::TestEventHandler overrides.
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (event->type() == ui::ET_MOUSE_PRESSED)
      ui::test::TestEventHandler::OnMouseEvent(event);
    event->StopPropagation();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TargetHitTestEventHandler);
};

// Test for http://crbug.com/297858. Virtual keyboard container should only show
// on primary root window.
TEST_F(VirtualKeyboardRootWindowControllerTest,
       VirtualKeyboardOnPrimaryRootWindowOnly) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x500,500x500");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* primary_root_window = Shell::GetPrimaryRootWindow();
  aura::Window* secondary_root_window =
      root_windows[0] == primary_root_window ?
          root_windows[1] : root_windows[0];

  ASSERT_TRUE(Shell::GetContainer(primary_root_window,
                                  kShellWindowId_VirtualKeyboardContainer));
  ASSERT_FALSE(Shell::GetContainer(secondary_root_window,
                                   kShellWindowId_VirtualKeyboardContainer));
}

// Test for http://crbug.com/263599. Virtual keyboard should be able to receive
// events at blocked user session.
TEST_F(VirtualKeyboardRootWindowControllerTest,
       ClickVirtualKeyboardInBlockedWindow) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  ASSERT_TRUE(keyboard_container);
  keyboard_container->Show();

  aura::Window* keyboard_window = keyboard::KeyboardController::GetInstance()->
      proxy()->GetKeyboardWindow();
  keyboard_container->AddChild(keyboard_window);
  keyboard_window->set_owned_by_parent(false);
  keyboard_window->SetBounds(gfx::Rect());
  keyboard_window->Show();

  ui::test::TestEventHandler handler;
  root_window->AddPreTargetHandler(&handler);

  ui::test::EventGenerator event_generator(root_window, keyboard_window);
  event_generator.ClickLeftButton();
  int expected_mouse_presses = 1;
  EXPECT_EQ(expected_mouse_presses, handler.num_mouse_events() / 2);

  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS;
       ++block_reason) {
    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));
    event_generator.ClickLeftButton();
    expected_mouse_presses++;
    EXPECT_EQ(expected_mouse_presses, handler.num_mouse_events() / 2);
    UnblockUserSession();
  }
  root_window->RemovePreTargetHandler(&handler);
}

// Test for http://crbug.com/299787. RootWindowController should delete
// the old container since the keyboard controller creates a new window in
// GetWindowContainer().
TEST_F(VirtualKeyboardRootWindowControllerTest,
       DeleteOldContainerOnVirtualKeyboardInit) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  ASSERT_TRUE(keyboard_container);
  // Track the keyboard container window.
  aura::WindowTracker tracker;
  tracker.Add(keyboard_container);
  // Mock a login user profile change to reinitialize the keyboard.
  ash::Shell::GetInstance()->OnLoginUserProfilePrepared();
  // keyboard_container should no longer be present.
  EXPECT_FALSE(tracker.Contains(keyboard_container));
}

// Test for crbug.com/342524. After user login, the work space should restore to
// full screen.
TEST_F(VirtualKeyboardRootWindowControllerTest, RestoreWorkspaceAfterLogin) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  keyboard_container->Show();
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  aura::Window* keyboard_window = controller->proxy()->GetKeyboardWindow();
  keyboard_container->AddChild(keyboard_window);
  keyboard_window->set_owned_by_parent(false);
  keyboard_window->SetBounds(keyboard::KeyboardBoundsFromWindowBounds(
      keyboard_container->bounds(), 100));
  keyboard_window->Show();

  gfx::Rect before = ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  // Notify keyboard bounds changing.
  controller->NotifyKeyboardBoundsChanging(
      controller->proxy()->GetKeyboardWindow()->bounds());

  if (!keyboard::IsKeyboardOverscrollEnabled()) {
    gfx::Rect after = ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
    EXPECT_LT(after, before);
  }

  // Mock a login user profile change to reinitialize the keyboard.
  ash::Shell::GetInstance()->OnLoginUserProfilePrepared();
  EXPECT_EQ(ash::Shell::GetScreen()->GetPrimaryDisplay().work_area(), before);
}

// Ensure that system modal dialogs do not block events targeted at the virtual
// keyboard.
TEST_F(VirtualKeyboardRootWindowControllerTest, ClickWithActiveModalDialog) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  ASSERT_TRUE(keyboard_container);
  keyboard_container->Show();

  aura::Window* keyboard_window = keyboard::KeyboardController::GetInstance()->
      proxy()->GetKeyboardWindow();
  keyboard_container->AddChild(keyboard_window);
  keyboard_window->set_owned_by_parent(false);
  keyboard_window->SetBounds(keyboard::KeyboardBoundsFromWindowBounds(
      keyboard_container->bounds(), 100));

  ui::test::TestEventHandler handler;
  root_window->AddPreTargetHandler(&handler);
  ui::test::EventGenerator root_window_event_generator(root_window);
  ui::test::EventGenerator keyboard_event_generator(root_window,
                                                    keyboard_window);

  views::Widget* modal_widget =
      CreateModalWidget(gfx::Rect(300, 10, 100, 100));

  // Verify that mouse events to the root window are block with a visble modal
  // dialog.
  root_window_event_generator.ClickLeftButton();
  EXPECT_EQ(0, handler.num_mouse_events());

  // Verify that event dispatch to the virtual keyboard is unblocked.
  keyboard_event_generator.ClickLeftButton();
  EXPECT_EQ(1, handler.num_mouse_events() / 2);

  modal_widget->Close();

  // Verify that mouse events are now unblocked to the root window.
  root_window_event_generator.ClickLeftButton();
  EXPECT_EQ(2, handler.num_mouse_events() / 2);
  root_window->RemovePreTargetHandler(&handler);
}

// Ensure that the visible area for scrolling the text caret excludes the
// region occluded by the on-screen keyboard.
TEST_F(VirtualKeyboardRootWindowControllerTest, EnsureCaretInWorkArea) {
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  keyboard::KeyboardControllerProxy* proxy = keyboard_controller->proxy();

  MockTextInputClient text_input_client;
  ui::InputMethod* input_method = proxy->GetInputMethod();
  ASSERT_TRUE(input_method);
  if (switches::IsTextInputFocusManagerEnabled()) {
    ui::TextInputFocusManager::GetInstance()->FocusTextInputClient(
        &text_input_client);
  } else {
    input_method->SetFocusedTextInputClient(&text_input_client);
  }

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  ASSERT_TRUE(keyboard_container);
  keyboard_container->Show();

  const int keyboard_height = 100;
  aura::Window* keyboard_window =proxy->GetKeyboardWindow();
  keyboard_container->AddChild(keyboard_window);
  keyboard_window->set_owned_by_parent(false);
  keyboard_window->SetBounds(keyboard::KeyboardBoundsFromWindowBounds(
      keyboard_container->bounds(), keyboard_height));

  proxy->EnsureCaretInWorkArea();
  ASSERT_EQ(keyboard_container->bounds().width(),
            text_input_client.visible_rect().width());
  ASSERT_EQ(keyboard_container->bounds().height() - keyboard_height,
            text_input_client.visible_rect().height());

  if (switches::IsTextInputFocusManagerEnabled()) {
    ui::TextInputFocusManager::GetInstance()->BlurTextInputClient(
        &text_input_client);
  } else {
    input_method->SetFocusedTextInputClient(NULL);
  }
}

// Tests that the virtual keyboard does not block context menus. The virtual
// keyboard should appear in front of most content, but not context menus. See
// crbug/377180.
TEST_F(VirtualKeyboardRootWindowControllerTest, ZOrderTest) {
  UpdateDisplay("800x600");
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  keyboard::KeyboardControllerProxy* proxy = keyboard_controller->proxy();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  ASSERT_TRUE(keyboard_container);
  keyboard_container->Show();

  const int keyboard_height = 200;
  aura::Window* keyboard_window = proxy->GetKeyboardWindow();
  keyboard_container->AddChild(keyboard_window);
  keyboard_window->set_owned_by_parent(false);
  gfx::Rect keyboard_bounds = keyboard::KeyboardBoundsFromWindowBounds(
      keyboard_container->bounds(), keyboard_height);
  keyboard_window->SetBounds(keyboard_bounds);
  keyboard_window->Show();

  ui::test::EventGenerator generator(root_window);

  // Cover the screen with two windows: a normal window on the left side and a
  // context menu on the right side. When the virtual keyboard is displayed it
  // partially occludes the normal window, but not the context menu. Compute
  // positions for generating synthetic click events to perform hit tests,
  // ensuring the correct window layering. 'top' is above the VK, whereas
  // 'bottom' lies within the VK. 'left' is centered in the normal window, and
  // 'right' is centered in the context menu.
  int window_height = keyboard_bounds.bottom();
  int window_width = keyboard_bounds.width() / 2;
  int left = window_width / 2;
  int right = 3 * window_width / 2;
  int top = keyboard_bounds.y() / 2;
  int bottom = window_height - keyboard_height / 2;

  // Normal window is partially occluded by the virtual keyboard.
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> normal(CreateTestWindowInShellWithDelegateAndType(
      &delegate,
      ui::wm::WINDOW_TYPE_NORMAL,
      0,
      gfx::Rect(0, 0, window_width, window_height)));
  normal->set_owned_by_parent(false);
  normal->Show();
  TargetHitTestEventHandler normal_handler;
  normal->AddPreTargetHandler(&normal_handler);

  // Test that only the click on the top portion of the window is picked up. The
  // click on the bottom hits the virtual keyboard instead.
  generator.MoveMouseTo(left, top);
  generator.ClickLeftButton();
  EXPECT_EQ(1, normal_handler.num_mouse_events());
  generator.MoveMouseTo(left, bottom);
  generator.ClickLeftButton();
  EXPECT_EQ(1, normal_handler.num_mouse_events());

  // Menu overlaps virtual keyboard.
  aura::test::TestWindowDelegate delegate2;
  scoped_ptr<aura::Window> menu(CreateTestWindowInShellWithDelegateAndType(
      &delegate2,
      ui::wm::WINDOW_TYPE_MENU,
      0,
      gfx::Rect(window_width, 0, window_width, window_height)));
  menu->set_owned_by_parent(false);
  menu->Show();
  TargetHitTestEventHandler menu_handler;
  menu->AddPreTargetHandler(&menu_handler);

  // Test that both clicks register.
  generator.MoveMouseTo(right, top);
  generator.ClickLeftButton();
  EXPECT_EQ(1, menu_handler.num_mouse_events());
  generator.MoveMouseTo(right, bottom);
  generator.ClickLeftButton();
  EXPECT_EQ(2, menu_handler.num_mouse_events());

  // Cleanup to ensure that the test windows are destroyed before their
  // delegates.
  normal.reset();
  menu.reset();
}

}  // namespace test
}  // namespace ash
