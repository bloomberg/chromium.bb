// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

namespace {

const int kVirtualKeyboardHeight = 100;

// A login implementation of WidgetDelegate.
class LoginTestWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit LoginTestWidgetDelegate(views::Widget* widget) : widget_(widget) {
  }
  virtual ~LoginTestWidgetDelegate() {}

  // Overridden from WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return widget_;
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return widget_;
  }
  virtual bool CanActivate() const OVERRIDE {
    return true;
  }
  virtual bool ShouldAdvanceFocusToTopLevelWidget() const OVERRIDE {
    return true;
  }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(LoginTestWidgetDelegate);
};

}  // namespace

class LockLayoutManagerTest : public AshTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    // Allow a virtual keyboard (and initialize it per default).
    CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    Shell::GetPrimaryRootWindowController()->ActivateKeyboard(
        keyboard::KeyboardController::GetInstance());
  }

  virtual void TearDown() OVERRIDE {
    Shell::GetPrimaryRootWindowController()->DeactivateKeyboard(
        keyboard::KeyboardController::GetInstance());
    AshTestBase::TearDown();
  }

  aura::Window* CreateTestLoginWindow(views::Widget::InitParams params,
                                      bool use_delegate) {
    aura::Window* parent = Shell::GetPrimaryRootWindowController()->
        GetContainer(ash::kShellWindowId_LockScreenContainer);
    params.parent = parent;
    views::Widget* widget = new views::Widget;
    if (use_delegate)
      params.delegate = new LoginTestWidgetDelegate(widget);
    widget->Init(params);
    widget->Show();
    aura::Window* window = widget->GetNativeView();
    return window;
  }

  // Show or hide the keyboard.
  void ShowKeyboard(bool show) {
    keyboard::KeyboardController* keyboard =
        keyboard::KeyboardController::GetInstance();
    ASSERT_TRUE(keyboard);
    if (show == keyboard->keyboard_visible())
      return;

    if (show) {
      keyboard->ShowKeyboard(true);
      if (keyboard->proxy()->GetKeyboardWindow()->bounds().height() == 0) {
        keyboard->proxy()->GetKeyboardWindow()->SetBounds(
            keyboard::KeyboardBoundsFromWindowBounds(
                keyboard->GetContainerWindow()->bounds(),
                kVirtualKeyboardHeight));
      }
    } else {
      keyboard->HideKeyboard(keyboard::KeyboardController::HIDE_REASON_MANUAL);
    }

    DCHECK_EQ(show, keyboard->keyboard_visible());
  }
};

TEST_F(LockLayoutManagerTest, NorwmalWindowBoundsArePreserved) {
  gfx::Rect screen_bounds = Shell::GetScreen()->GetPrimaryDisplay().bounds();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  const gfx::Rect bounds = gfx::Rect(10, 10, 300, 300);
  widget_params.bounds = bounds;
  scoped_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));
  EXPECT_EQ(bounds.ToString(), window->GetBoundsInScreen().ToString());

  gfx::Rect work_area =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(window.get());
  window->SetBounds(work_area);

  EXPECT_EQ(work_area.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_NE(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());

  const gfx::Rect bounds2 = gfx::Rect(100, 100, 200, 200);
  window->SetBounds(bounds2);
  EXPECT_EQ(bounds2.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockLayoutManagerTest, MaximizedFullscreenWindowBoundsAreEqualToScreen) {
  gfx::Rect screen_bounds = Shell::GetScreen()->GetPrimaryDisplay().bounds();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  const gfx::Rect bounds = gfx::Rect(10, 10, 300, 300);
  widget_params.bounds = bounds;
  // Maximized TYPE_WINDOW_FRAMELESS windows needs a delegate defined otherwise
  // it won't get initial SetBounds event.
  scoped_ptr<aura::Window> maximized_window(
      CreateTestLoginWindow(widget_params, true /* use_delegate */));

  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  widget_params.delegate = NULL;
  scoped_ptr<aura::Window> fullscreen_window(
      CreateTestLoginWindow(widget_params, false  /* use_delegate */));

  EXPECT_EQ(screen_bounds.ToString(),
            maximized_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());

  gfx::Rect work_area =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(maximized_window.get());
  maximized_window->SetBounds(work_area);

  EXPECT_NE(work_area.ToString(),
            maximized_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            maximized_window->GetBoundsInScreen().ToString());

  work_area =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(fullscreen_window.get());
  fullscreen_window->SetBounds(work_area);
  EXPECT_NE(work_area.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());

  const gfx::Rect bounds2 = gfx::Rect(100, 100, 200, 200);
  maximized_window->SetBounds(bounds2);
  fullscreen_window->SetBounds(bounds2);
  EXPECT_EQ(screen_bounds.ToString(),
            maximized_window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(),
            fullscreen_window->GetBoundsInScreen().ToString());
}

TEST_F(LockLayoutManagerTest, KeyboardBounds) {
  gfx::Rect screen_bounds = Shell::GetScreen()->GetPrimaryDisplay().bounds();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  scoped_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));

  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // When virtual keyboard overscroll is enabled keyboard bounds should not
  // affect window bounds.
  keyboard::SetKeyboardOverscrollOverride(
       keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_ENABLED);
  ShowKeyboard(true);
  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());
  ShowKeyboard(false);

  // When virtual keyboard overscroll is disabled keyboard bounds do
  // affect window bounds.
  keyboard::SetKeyboardOverscrollOverride(
       keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_DISABLED);
  ShowKeyboard(true);
  keyboard::KeyboardController* keyboard =
        keyboard::KeyboardController::GetInstance();
  gfx::Rect target_bounds(screen_bounds);
  target_bounds.set_height(target_bounds.height() -
      keyboard->proxy()->GetKeyboardWindow()->bounds().height());
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
  ShowKeyboard(false);

  keyboard::SetKeyboardOverscrollOverride(
      keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_NONE);
}

TEST_F(LockLayoutManagerTest, MultipleMonitors) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x400,400x500");
  gfx::Rect screen_bounds = Shell::GetScreen()->GetPrimaryDisplay().bounds();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  scoped_ptr<aura::Window> window(
      CreateTestLoginWindow(widget_params, false /* use_delegate */));
  window->SetProperty(aura::client::kCanMaximizeKey, true);

  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));

  // Maximize the window with as the restore bounds is inside 2nd display but
  // lock container windows are always on primary display.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  window_state->SetRestoreBoundsInScreen(gfx::Rect(280, 0, 30, 40));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ("0,0 300x400", window->GetBoundsInScreen().ToString());

  gfx::Rect work_area =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(window.get());
  window->SetBounds(work_area);
  // Usually work_area takes Shelf into account but that doesn't affect
  // LockScreen container windows.
  EXPECT_NE(work_area.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(screen_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

}  // namespace test
}  // namespace ash
