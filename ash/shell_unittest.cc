// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::RootWindow;

namespace ash {

namespace {

views::Widget* CreateTestWindow(const views::Widget::InitParams& params) {
  views::Widget* widget = new views::Widget;
  widget->Init(params);
  return widget;
}

aura::Window* GetDefaultContainer() {
  return Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_DefaultContainer);
}

aura::Window* GetAlwaysOnTopContainer() {
  return Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_AlwaysOnTopContainer);
}

// Expect ALL the containers!
void ExpectAllContainers() {
  Shell* shell = Shell::GetInstance();
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_DesktopBackgroundContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_DefaultContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_AlwaysOnTopContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_PanelContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_LauncherContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_SystemModalContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_LockScreenContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_LockSystemModalContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_StatusContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_MenuContainer));
  EXPECT_TRUE(shell->GetContainer(
      internal::kShellWindowId_DragImageAndTooltipContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_SettingBubbleContainer));
  EXPECT_TRUE(
      shell->GetContainer(internal::kShellWindowId_OverlayContainer));
}

void TestCreateWindow(views::Widget::InitParams::Type type,
                      bool always_on_top,
                      aura::Window* expected_container) {
  views::Widget::InitParams widget_params(type);
  widget_params.keep_on_top = always_on_top;

  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  EXPECT_EQ(expected_container, widget->GetNativeWindow()->parent()) <<
      "TestCreateWindow: type=" << type << ", always_on_top=" << always_on_top;

  widget->Close();
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
  virtual string16 GetWindowTitle() const OVERRIDE {
    return ASCIIToUTF16("Modal Window");
  }
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_SYSTEM;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

}  // namespace

class ShellTest : public test::AshTestBase {
 public:
  ShellTest() {}
  virtual ~ShellTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellTest);
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
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(true);
  // And it should in always on top container now.
  EXPECT_EQ(GetAlwaysOnTopContainer(), widget->GetNativeWindow()->parent());

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(false);
  // It should go back to default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Set the same always-on-top flag again.
  widget->SetAlwaysOnTop(false);
  // Should have no effect and we are still in the default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  widget->Close();
}

TEST_F(ShellTest, CreateModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Create a modal window.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in modal container.
  aura::Window* modal_container = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_SystemModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  modal_widget->Close();
  widget->Close();
}

TEST_F(ShellTest, CreateLockScreenModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWindow(widget_params);
  ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_LockScreenContainer)->
      AddChild(lock_widget->GetNativeView());
  lock_widget->Show();

  // It should be in LockScreen container.
  aura::Window* lock_screen = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_LockScreenContainer);
  EXPECT_EQ(lock_screen, lock_widget->GetNativeWindow()->parent());

  // Create a modal window with a lock window as parent.
  views::Widget* lock_modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), lock_widget->GetNativeView());
  lock_modal_widget->Show();

  // It should be in LockScreen modal container.
  aura::Window* lock_modal_container = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_LockSystemModalContainer);
  EXPECT_EQ(lock_modal_container,
            lock_modal_widget->GetNativeWindow()->parent());

  // Create a modal window with a normal window as parent.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in non-LockScreen modal container.
  aura::Window* modal_container = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_SystemModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  modal_widget->Close();
  lock_modal_widget->Close();
  lock_widget->Close();
  widget->Close();
}

TEST_F(ShellTest, IsScreenLocked) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // A normal window does not lock the screen.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());
  widget->Hide();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());

  // A modal window with a normal window as parent does not locks the screen.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());
  modal_widget->Close();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());
  widget->Close();

  // A lock screen window locks the screen.
  views::Widget* lock_widget = CreateTestWindow(widget_params);
  ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_LockScreenContainer)->
      AddChild(lock_widget->GetNativeView());
  lock_widget->Show();
  EXPECT_TRUE(Shell::GetInstance()->IsScreenLocked());
  lock_widget->Hide();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());

  // A modal window with a lock window as parent does not lock the screen. The
  // screen is locked only when a lock window is visible.
  views::Widget* lock_modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), lock_widget->GetNativeView());
  lock_modal_widget->Show();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());
  lock_widget->Show();
  EXPECT_TRUE(Shell::GetInstance()->IsScreenLocked());
  lock_modal_widget->Close();
  EXPECT_TRUE(Shell::GetInstance()->IsScreenLocked());
  lock_widget->Close();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());
}

TEST_F(ShellTest, ComputeWindowMode) {
  // By default, we use managed mode.
  Shell* shell = Shell::GetInstance();
  Shell::TestApi test_api(shell);
  CommandLine command_line_blank(CommandLine::NO_PROGRAM);
  EXPECT_EQ(Shell::MODE_MANAGED,
            test_api.ComputeWindowMode(&command_line_blank));

  // Sometimes we force compact mode.
  CommandLine command_line_force(CommandLine::NO_PROGRAM);
  command_line_force.AppendSwitch(switches::kAuraForceCompactWindowMode);
  EXPECT_EQ(Shell::MODE_COMPACT,
            test_api.ComputeWindowMode(&command_line_force));

  // The user can set compact mode.
  CommandLine command_line_compact(CommandLine::NO_PROGRAM);
  command_line_compact.AppendSwitchASCII(switches::kAuraWindowMode,
                                         switches::kAuraWindowModeCompact);
  EXPECT_EQ(Shell::MODE_COMPACT,
            test_api.ComputeWindowMode(&command_line_compact));

  // The user can set managed.
  CommandLine command_line_managed(CommandLine::NO_PROGRAM);
  command_line_managed.AppendSwitchASCII(switches::kAuraWindowMode,
                                         switches::kAuraWindowModeManaged);
  EXPECT_EQ(Shell::MODE_MANAGED,
            test_api.ComputeWindowMode(&command_line_managed));
}

// Fails on Mac, see http://crbug.com/115662
#if defined(OS_MACOSX)
#define MAYBE_ManagedWindowModeBasics FAILS_ManagedWindowModeBasics
#else
#define MAYBE_ManagedWindowModeBasics ManagedWindowModeBasics
#endif
TEST_F(ShellTest, MAYBE_ManagedWindowModeBasics) {
  Shell* shell = Shell::GetInstance();
  Shell::TestApi test_api(shell);

  // We start with the usual window containers.
  ExpectAllContainers();
  // We're not in compact window mode by default.
  EXPECT_FALSE(shell->IsWindowModeCompact());
  // We have a default container event filter (for window drags).
  EXPECT_TRUE(GetDefaultContainer()->event_filter());
  // Launcher is visible.
  views::Widget* launcher_widget = shell->launcher()->widget();
  EXPECT_TRUE(launcher_widget->IsVisible());
  // Launcher is at bottom-left of screen.
  EXPECT_EQ(0, launcher_widget->GetWindowScreenBounds().x());
  EXPECT_EQ(Shell::GetRootWindow()->GetHostSize().height(),
            launcher_widget->GetWindowScreenBounds().bottom());
  // We have a desktop background but not a bare layer.
  EXPECT_TRUE(test_api.root_window_layout()->background_widget());
  EXPECT_FALSE(test_api.root_window_layout()->background_layer());

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
  EXPECT_TRUE(Shell::GetInstance()->shelf()->visible());

  // Fullscreen window hides it.
  widget->SetFullscreen(true);
  EXPECT_FALSE(Shell::GetInstance()->shelf()->visible());

  // Restoring the window restores it.
  widget->Restore();
  EXPECT_TRUE(Shell::GetInstance()->shelf()->visible());

  // Clean up.
  widget->Close();
}

// By implementing GetOverrideWindowMode we make the Shell come up in compact
// window mode.
class ShellCompactWindowModeTest : public test::AshTestBase {
 public:
  ShellCompactWindowModeTest() {}
  virtual ~ShellCompactWindowModeTest() {}

 protected:
  virtual bool GetOverrideWindowMode(Shell::WindowMode* window_mode) {
    *window_mode = Shell::MODE_COMPACT;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellCompactWindowModeTest);
};

TEST_F(ShellCompactWindowModeTest, CompactWindowModeBasics) {
  Shell* shell = Shell::GetInstance();
  Shell::TestApi test_api(shell);

  EXPECT_TRUE(shell->IsWindowModeCompact());
  // Compact mode does not use a default container event filter.
  EXPECT_FALSE(GetDefaultContainer()->event_filter());
  // We have all the usual containers.
  ExpectAllContainers();

  // Compact mode has no shelf.
  EXPECT_TRUE(shell->shelf() == NULL);

  // Create a window.  In compact mode, windows are created maximized.
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.bounds.SetRect(11, 22, 300, 400);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // Window bounds got updated to fill the work area.
  EXPECT_EQ(widget->GetWorkAreaBoundsInScreen(),
            widget->GetWindowScreenBounds());
  // Launcher is hidden.
  views::Widget* launcher_widget = shell->launcher()->widget();
  EXPECT_FALSE(launcher_widget->IsVisible());
  // Desktop background widget is gone but we have a layer.
  EXPECT_FALSE(test_api.root_window_layout()->background_widget());
  EXPECT_TRUE(test_api.root_window_layout()->background_layer());

  // Clean up.
  widget->Close();
}

}  // namespace ash
