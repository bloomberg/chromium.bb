// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
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
  return Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      internal::kShellWindowId_DefaultContainer);
}

aura::Window* GetAlwaysOnTopContainer() {
  return Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      internal::kShellWindowId_AlwaysOnTopContainer);
}

// Expect ALL the containers!
void ExpectAllContainers() {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_DesktopBackgroundContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_SystemBackgroundContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_DefaultContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_AlwaysOnTopContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_PanelContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_LauncherContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_SystemModalContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_LockScreenBackgroundContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_LockScreenContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_LockSystemModalContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_StatusContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_MenuContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_DragImageAndTooltipContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_SettingBubbleContainer));
  EXPECT_TRUE(Shell::GetContainer(
      root_window, internal::kShellWindowId_OverlayContainer));
}

void TestCreateWindow(views::Widget::InitParams::Type type,
                      bool always_on_top,
                      aura::Window* expected_container) {
  views::Widget::InitParams widget_params(type);
  widget_params.keep_on_top = always_on_top;

  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  EXPECT_TRUE(expected_container->Contains(
                  widget->GetNativeWindow()->parent())) <<
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

typedef test::AshTestBase ShellTest;

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
      Shell::GetPrimaryRootWindow(),
      internal::kShellWindowId_SystemModalContainer);
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
  EXPECT_TRUE(GetDefaultContainer()->Contains(
                  widget->GetNativeWindow()->parent()));

  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWindow(widget_params);
  ash::Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      ash::internal::kShellWindowId_LockScreenContainer)->
      AddChild(lock_widget->GetNativeView());
  lock_widget->Show();

  // It should be in LockScreen container.
  aura::Window* lock_screen = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      ash::internal::kShellWindowId_LockScreenContainer);
  EXPECT_EQ(lock_screen, lock_widget->GetNativeWindow()->parent());

  // Create a modal window with a lock window as parent.
  views::Widget* lock_modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), lock_widget->GetNativeView());
  lock_modal_widget->Show();

  // It should be in LockScreen modal container.
  aura::Window* lock_modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      ash::internal::kShellWindowId_LockSystemModalContainer);
  EXPECT_EQ(lock_modal_container,
            lock_modal_widget->GetNativeWindow()->parent());

  // Create a modal window with a normal window as parent.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in non-LockScreen modal container.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      ash::internal::kShellWindowId_SystemModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  modal_widget->Close();
  lock_modal_widget->Close();
  lock_widget->Close();
  widget->Close();
}

TEST_F(ShellTest, IsScreenLocked) {
  ash::Shell::GetInstance()->delegate()->LockScreen();
  EXPECT_TRUE(Shell::GetInstance()->IsScreenLocked());
  ash::Shell::GetInstance()->delegate()->UnlockScreen();
  EXPECT_FALSE(Shell::GetInstance()->IsScreenLocked());
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
  // Launcher is visible.
  views::Widget* launcher_widget = shell->launcher()->widget();
  EXPECT_TRUE(launcher_widget->IsVisible());
  // Launcher is at bottom-left of screen.
  EXPECT_EQ(0, launcher_widget->GetWindowBoundsInScreen().x());
  EXPECT_EQ(Shell::GetPrimaryRootWindow()->GetHostSize().height(),
            launcher_widget->GetWindowBoundsInScreen().bottom());
  // We have a desktop background but not a bare layer.
  // TODO (antrim): enable once we find out why it fails component build.
  //  internal::DesktopBackgroundWidgetController* background =
  //      Shell::GetPrimaryRootWindow()->
  //          GetProperty(internal::kWindowDesktopComponent);
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
  EXPECT_EQ(internal::ShelfLayoutManager::VISIBLE,
            Shell::GetInstance()->shelf()->visibility_state());

  // Fullscreen window hides it.
  widget->SetFullscreen(true);
  EXPECT_EQ(internal::ShelfLayoutManager::HIDDEN,
            Shell::GetInstance()->shelf()->visibility_state());

  // Restoring the window restores it.
  widget->Restore();
  EXPECT_EQ(internal::ShelfLayoutManager::VISIBLE,
            Shell::GetInstance()->shelf()->visibility_state());

  // Clean up.
  widget->Close();
}

namespace {

// Builds the list of parents from |window| to the root. The returned vector is
// in reverse order (|window| is first).
std::vector<aura::Window*> BuildPathToRoot(aura::Window* window) {
  std::vector<aura::Window*> results;
  while (window) {
    results.push_back(window);
    window = window->parent();
  }
  return results;
}

}  // namespace

// The SystemBackgroundContainer needs to be behind the
// DesktopBackgroundContainer, otherwise workspace animations don't line up.
TEST_F(ShellTest, SystemBackgroundBehindDesktopBackground) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* desktop = Shell::GetContainer(
      root_window, internal::kShellWindowId_DesktopBackgroundContainer);
  ASSERT_TRUE(desktop != NULL);
  aura::Window* system_bg = Shell::GetContainer(
      root_window, internal::kShellWindowId_SystemBackgroundContainer);
  ASSERT_TRUE(system_bg != NULL);

  std::vector<aura::Window*> desktop_parents(BuildPathToRoot(desktop));
  std::vector<aura::Window*> system_bg_parents(BuildPathToRoot(system_bg));

  for (size_t i = 0; i < system_bg_parents.size(); ++i) {
    std::vector<aura::Window*>::iterator desktop_i =
        std::find(desktop_parents.begin(), desktop_parents.end(),
                  system_bg_parents[i]);
    if (desktop_i != desktop_parents.end()) {
      // Found the common parent.
      ASSERT_NE(0u, i);
      ASSERT_TRUE(desktop_i != desktop_parents.begin());
      aura::Window* common_parent = system_bg_parents[i];
      int system_child = static_cast<int>(std::find(
          common_parent->children().begin(),
          common_parent->children().end(), system_bg_parents[i - 1]) -
          common_parent->children().begin());
      int desktop_child = static_cast<int>(std::find(
          common_parent->children().begin(),
          common_parent->children().end(), *(desktop_i - 1)) -
          common_parent->children().begin());
      EXPECT_LT(system_child, desktop_child);
      return;
    }
  }
  EXPECT_TRUE(false) <<
      "system background and desktop background need to have a common parent";
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
  window_->Init(ui::LAYER_NOT_DRAWN);
}

}  // namespace ash
