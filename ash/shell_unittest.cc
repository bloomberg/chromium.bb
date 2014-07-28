// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/window_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

using aura::RootWindow;

namespace ash {

namespace {

aura::Window* GetDefaultContainer() {
  return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             kShellWindowId_DefaultContainer);
}

aura::Window* GetAlwaysOnTopContainer() {
  return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             kShellWindowId_AlwaysOnTopContainer);
}

// Expect ALL the containers!
void ExpectAllContainers() {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_DesktopBackgroundContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_DefaultContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window, kShellWindowId_PanelContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window, kShellWindowId_ShelfContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_SystemModalContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, kShellWindowId_LockScreenBackgroundContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_LockScreenContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_LockSystemModalContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window, kShellWindowId_StatusContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window, kShellWindowId_MenuContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_DragImageAndTooltipContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_SettingBubbleContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, kShellWindowId_VirtualKeyboardParentContainer));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_MouseCursorContainer));
#endif
}

class ModalWindow : public views::WidgetDelegateView {
 public:
  ModalWindow() {}
  virtual ~ModalWindow() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return base::ASCIIToUTF16("Modal Window");
  }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_SYSTEM;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

class SimpleMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  SimpleMenuDelegate() {}
  virtual ~SimpleMenuDelegate() {}

  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }

  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleMenuDelegate);
};

}  // namespace

class ShellTest : public test::AshTestBase {
 public:
  views::Widget* CreateTestWindow(views::Widget::InitParams params) {
    views::Widget* widget = new views::Widget;
    params.context = CurrentContext();
    widget->Init(params);
    return widget;
  }

  void TestCreateWindow(views::Widget::InitParams::Type type,
                        bool always_on_top,
                        aura::Window* expected_container) {
    views::Widget::InitParams widget_params(type);
    widget_params.keep_on_top = always_on_top;

    views::Widget* widget = CreateTestWindow(widget_params);
    widget->Show();

    EXPECT_TRUE(
        expected_container->Contains(widget->GetNativeWindow()->parent())) <<
        "TestCreateWindow: type=" << type << ", always_on_top=" <<
        always_on_top;

    widget->Close();
  }

  void LockScreenAndVerifyMenuClosed() {
    // Verify a menu is open before locking.
    views::MenuController* menu_controller =
        views::MenuController::GetActiveInstance();
    DCHECK(menu_controller);
    EXPECT_EQ(views::MenuController::EXIT_NONE, menu_controller->exit_type());

    // Create a LockScreen window.
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW);
    SessionStateDelegate* delegate =
        Shell::GetInstance()->session_state_delegate();
    delegate->LockScreen();
    views::Widget* lock_widget = CreateTestWindow(widget_params);
    ash::Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             ash::kShellWindowId_LockScreenContainer)
        ->AddChild(lock_widget->GetNativeView());
    lock_widget->Show();
    EXPECT_TRUE(delegate->IsScreenLocked());
    EXPECT_TRUE(lock_widget->GetNativeView()->HasFocus());

    // Verify menu is closed.
    EXPECT_NE(views::MenuController::EXIT_NONE, menu_controller->exit_type());
    lock_widget->Close();
    delegate->UnlockScreen();

    // In case the menu wasn't closed, cancel the menu to exit the nested menu
    // run loop so that the test will not time out.
    menu_controller->CancelAll();
  }
};

TEST_F(ShellTest, CreateWindow) {
  // Normal window should be created in default container.
  TestCreateWindow(views::Widget::InitParams::TYPE_WINDOW,
                   false,  // always_on_top
                   GetDefaultContainer());
  TestCreateWindow(views::Widget::InitParams::TYPE_POPUP,
                   false,  // always_on_top
                   GetDefaultContainer());

  // Always-on-top window and popup are created in always-on-top container.
  TestCreateWindow(views::Widget::InitParams::TYPE_WINDOW,
                   true,  // always_on_top
                   GetAlwaysOnTopContainer());
  TestCreateWindow(views::Widget::InitParams::TYPE_POPUP,
                   true,  // always_on_top
                   GetAlwaysOnTopContainer());
}

TEST_F(ShellTest, ChangeAlwaysOnTop) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Creates a normal window
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_TRUE(GetDefaultContainer()->Contains(
                  widget->GetNativeWindow()->parent()));

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(true);
  // And it should in always on top container now.
  EXPECT_EQ(GetAlwaysOnTopContainer(), widget->GetNativeWindow()->parent());

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(false);
  // It should go back to default container.
  EXPECT_TRUE(GetDefaultContainer()->Contains(
                  widget->GetNativeWindow()->parent()));

  // Set the same always-on-top flag again.
  widget->SetAlwaysOnTop(false);
  // Should have no effect and we are still in the default container.
  EXPECT_TRUE(GetDefaultContainer()->Contains(
                  widget->GetNativeWindow()->parent()));

  widget->Close();
}

TEST_F(ShellTest, CreateModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_TRUE(GetDefaultContainer()->Contains(
                  widget->GetNativeWindow()->parent()));

  // Create a modal window.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in modal container.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SystemModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  modal_widget->Close();
  widget->Close();
}

class TestModalDialogDelegate : public views::DialogDelegateView {
 public:
  TestModalDialogDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_SYSTEM;
  }
};

TEST_F(ShellTest, CreateLockScreenModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();
  EXPECT_TRUE(widget->GetNativeView()->HasFocus());

  // It should be in default container.
  EXPECT_TRUE(GetDefaultContainer()->Contains(
                  widget->GetNativeWindow()->parent()));

  Shell::GetInstance()->session_state_delegate()->LockScreen();
  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWindow(widget_params);
  ash::Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                           ash::kShellWindowId_LockScreenContainer)
      ->AddChild(lock_widget->GetNativeView());
  lock_widget->Show();
  EXPECT_TRUE(lock_widget->GetNativeView()->HasFocus());

  // It should be in LockScreen container.
  aura::Window* lock_screen = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), ash::kShellWindowId_LockScreenContainer);
  EXPECT_EQ(lock_screen, lock_widget->GetNativeWindow()->parent());

  // Create a modal window with a lock window as parent.
  views::Widget* lock_modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), lock_widget->GetNativeView());
  lock_modal_widget->Show();
  EXPECT_TRUE(lock_modal_widget->GetNativeView()->HasFocus());

  // It should be in LockScreen modal container.
  aura::Window* lock_modal_container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          ash::kShellWindowId_LockSystemModalContainer);
  EXPECT_EQ(lock_modal_container,
            lock_modal_widget->GetNativeWindow()->parent());

  // Create a modal window with a normal window as parent.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();
  // Window on lock screen shouldn't lost focus.
  EXPECT_FALSE(modal_widget->GetNativeView()->HasFocus());
  EXPECT_TRUE(lock_modal_widget->GetNativeView()->HasFocus());

  // It should be in non-LockScreen modal container.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), ash::kShellWindowId_SystemModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  // Modal dialog without parent, caused crash see crbug.com/226141
  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      new TestModalDialogDelegate(), CurrentContext(), NULL);

  modal_dialog->Show();
  EXPECT_FALSE(modal_dialog->GetNativeView()->HasFocus());
  EXPECT_TRUE(lock_modal_widget->GetNativeView()->HasFocus());

  modal_dialog->Close();
  modal_widget->Close();
  modal_widget->Close();
  lock_modal_widget->Close();
  lock_widget->Close();
  widget->Close();
}

TEST_F(ShellTest, IsScreenLocked) {
  SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();
  delegate->LockScreen();
  EXPECT_TRUE(delegate->IsScreenLocked());
  delegate->UnlockScreen();
  EXPECT_FALSE(delegate->IsScreenLocked());
}

TEST_F(ShellTest, LockScreenClosesActiveMenu) {
  SimpleMenuDelegate menu_delegate;
  scoped_ptr<ui::SimpleMenuModel> menu_model(
      new ui::SimpleMenuModel(&menu_delegate));
  menu_model->AddItem(0, base::ASCIIToUTF16("Menu item"));
  views::Widget* widget = ash::Shell::GetPrimaryRootWindowController()->
      wallpaper_controller()->widget();
  scoped_ptr<views::MenuRunner> menu_runner(
      new views::MenuRunner(menu_model.get(), views::MenuRunner::CONTEXT_MENU));

  // When MenuRunner runs a nested loop the LockScreenAndVerifyMenuClosed
  // command will fire, check the menu state and ensure the nested menu loop
  // is exited so that the test will terminate.
  base::MessageLoopForUI::current()->PostTask(FROM_HERE,
      base::Bind(&ShellTest::LockScreenAndVerifyMenuClosed,
                 base::Unretained(this)));

  EXPECT_EQ(views::MenuRunner::NORMAL_EXIT,
            menu_runner->RunMenuAt(widget,
                                   NULL,
                                   gfx::Rect(),
                                   views::MENU_ANCHOR_TOPLEFT,
                                   ui::MENU_SOURCE_MOUSE));
}

TEST_F(ShellTest, ManagedWindowModeBasics) {
  // We start with the usual window containers.
  ExpectAllContainers();
  // Shelf is visible.
  ShelfWidget* shelf_widget = Shelf::ForPrimaryDisplay()->shelf_widget();
  EXPECT_TRUE(shelf_widget->IsVisible());
  // Shelf is at bottom-left of screen.
  EXPECT_EQ(0, shelf_widget->GetWindowBoundsInScreen().x());
  EXPECT_EQ(Shell::GetPrimaryRootWindow()->GetHost()->GetBounds().height(),
            shelf_widget->GetWindowBoundsInScreen().bottom());
  // We have a desktop background but not a bare layer.
  // TODO (antrim): enable once we find out why it fails component build.
  //  DesktopBackgroundWidgetController* background =
  //      Shell::GetPrimaryRootWindow()->
  //          GetProperty(kWindowDesktopComponent);
  //  EXPECT_TRUE(background);
  //  EXPECT_TRUE(background->widget());
  //  EXPECT_FALSE(background->layer());

  // Create a normal window.  It is not maximized.
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.bounds.SetRect(11, 22, 300, 400);
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();
  EXPECT_FALSE(widget->IsMaximized());

  // Clean up.
  widget->Close();
}

TEST_F(ShellTest, FullscreenWindowHidesShelf) {
  ExpectAllContainers();

  // Create a normal window.  It is not maximized.
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.bounds.SetRect(11, 22, 300, 400);
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();
  EXPECT_FALSE(widget->IsMaximized());

  // Shelf defaults to visible.
  EXPECT_EQ(
      SHELF_VISIBLE,
      Shell::GetPrimaryRootWindowController()->
          GetShelfLayoutManager()->visibility_state());

  // Fullscreen window hides it.
  widget->SetFullscreen(true);
  EXPECT_EQ(
      SHELF_HIDDEN,
      Shell::GetPrimaryRootWindowController()->
          GetShelfLayoutManager()->visibility_state());

  // Restoring the window restores it.
  widget->Restore();
  EXPECT_EQ(
      SHELF_VISIBLE,
      Shell::GetPrimaryRootWindowController()->
          GetShelfLayoutManager()->visibility_state());

  // Clean up.
  widget->Close();
}

// Various assertions around SetShelfAutoHideBehavior() and
// GetShelfAutoHideBehavior().
TEST_F(ShellTest, ToggleAutoHide) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_TEXTURED);
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();
  wm::ActivateWindow(window.get());

  Shell* shell = Shell::GetInstance();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  shell->SetShelfAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
                                  root_window);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            shell->GetShelfAutoHideBehavior(root_window));
  shell->SetShelfAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
                                  root_window);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            shell->GetShelfAutoHideBehavior(root_window));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            shell->GetShelfAutoHideBehavior(root_window));
  shell->SetShelfAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
                                  root_window);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            shell->GetShelfAutoHideBehavior(root_window));
  shell->SetShelfAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
                                  root_window);
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
            shell->GetShelfAutoHideBehavior(root_window));
}

// Tests that the cursor-filter is ahead of the drag-drop controller in the
// pre-target list.
TEST_F(ShellTest, TestPreTargetHandlerOrder) {
  Shell* shell = Shell::GetInstance();
  ui::EventTargetTestApi test_api(shell);
  test::ShellTestApi shell_test_api(shell);

  const ui::EventHandlerList& handlers = test_api.pre_target_handlers();
  ui::EventHandlerList::const_iterator cursor_filter =
      std::find(handlers.begin(), handlers.end(), shell->mouse_cursor_filter());
  ui::EventHandlerList::const_iterator drag_drop =
      std::find(handlers.begin(), handlers.end(),
                shell_test_api.drag_drop_controller());
  EXPECT_NE(handlers.end(), cursor_filter);
  EXPECT_NE(handlers.end(), drag_drop);
  EXPECT_GT(drag_drop, cursor_filter);
}

// Verifies an EventHandler added to Env gets notified from EventGenerator.
TEST_F(ShellTest, EnvPreTargetHandler) {
  ui::test::TestEventHandler event_handler;
  aura::Env::GetInstance()->AddPreTargetHandler(&event_handler);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseBy(1, 1);
  EXPECT_NE(0, event_handler.num_mouse_events());
  aura::Env::GetInstance()->RemovePreTargetHandler(&event_handler);
}

// This verifies WindowObservers are removed when a window is destroyed after
// the Shell is destroyed. This scenario (aura::Windows being deleted after the
// Shell) occurs if someone is holding a reference to an unparented Window, as
// is the case with a RenderWidgetHostViewAura that isn't on screen. As long as
// everything is ok, we won't crash. If there is a bug, window's destructor will
// notify some deleted object (say VideoDetector or ActivationController) and
// this will crash.
class ShellTest2 : public test::AshTestBase {
 public:
  ShellTest2() {}
  virtual ~ShellTest2() {}

 protected:
  scoped_ptr<aura::Window> window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellTest2);
};

TEST_F(ShellTest2, DontCrashWhenWindowDeleted) {
  window_.reset(new aura::Window(NULL));
  window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
}

}  // namespace ash
