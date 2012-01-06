// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/aura_shell_test_base.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

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

// After base::AutoReset<> but via setter and getter.
class AutoResetUseFullscreenHostWindow {
 public:
  AutoResetUseFullscreenHostWindow(bool new_value) {
    old_value_ = aura::RootWindow::use_fullscreen_host_window();
    aura::RootWindow::set_use_fullscreen_host_window(new_value);
  }
  ~AutoResetUseFullscreenHostWindow() {
    aura::RootWindow::set_use_fullscreen_host_window(old_value_);
  }
 private:
  bool old_value_;
};

}  // namespace

class ShellTest : public test::AuraShellTestBase {
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
      ash::internal::kShellWindowId_ModalContainer);
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
      ash::internal::kShellWindowId_LockModalContainer);
  EXPECT_EQ(lock_modal_container,
            lock_modal_widget->GetNativeWindow()->parent());

  // Create a modal window with a normal window as parent.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in non-LockScreen modal container.
  aura::Window* modal_container = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_ModalContainer);
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
  // screen is locked only when a lock windown is visible.
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
  // We only change default window mode with full-screen host windows.
  AutoResetUseFullscreenHostWindow use_fullscreen_host_window(true);

  // Wide screens use normal window mode.
  Shell* shell = Shell::GetInstance();
  gfx::Size monitor_size(1440, 900);
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_EQ(Shell::NORMAL_MODE,
            shell->ComputeWindowMode(monitor_size, &command_line));

  // Alex-sized screens need compact mode.
  monitor_size.SetSize(1280, 800);
  EXPECT_EQ(Shell::COMPACT_MODE,
            shell->ComputeWindowMode(monitor_size, &command_line));

  // ZGB-sized screens need compact mode.
  monitor_size.SetSize(1366, 768);
  EXPECT_EQ(Shell::COMPACT_MODE,
            shell->ComputeWindowMode(monitor_size, &command_line));

  // Even for a small screen, the user can force normal mode.
  monitor_size.SetSize(800, 600);
  command_line.AppendSwitchASCII(ash::switches::kAuraWindowMode,
                                 ash::switches::kAuraWindowModeNormal);
  EXPECT_EQ(Shell::NORMAL_MODE,
            shell->ComputeWindowMode(monitor_size, &command_line));

  // Even for a large screen, the user can force compact mode.
  monitor_size.SetSize(1920, 1080);
  CommandLine command_line2(CommandLine::NO_PROGRAM);
  command_line2.AppendSwitchASCII(ash::switches::kAuraWindowMode,
                                 ash::switches::kAuraWindowModeCompact);
  EXPECT_EQ(Shell::COMPACT_MODE,
            shell->ComputeWindowMode(monitor_size, &command_line2));
}

}  // namespace ash
