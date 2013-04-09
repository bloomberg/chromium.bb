// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/test/base/ui_controls.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

class AshFocusCycleTest : public CrosInProcessBrowserTest {
 protected:
  AshFocusCycleTest() {}
  virtual ~AshFocusCycleTest() {}

  void CheckFocusedViewIsAccessible() {
    SCOPED_TRACE(base::StringPrintf(
        "while active window is %s",
        ash::wm::GetActiveWindow()->name().c_str()));
    aura::Window* window = ash::wm::GetActiveWindow();
    CHECK(window);
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    CHECK(widget);
    views::FocusManager* focus_manager = widget->GetFocusManager();
    CHECK(focus_manager);
    views::View* view = focus_manager->GetFocusedView();
    ASSERT_TRUE(view != NULL);
    ui::AccessibleViewState state;
    view->GetAccessibleState(&state);
    ASSERT_NE(state.role, ui::AccessibilityTypes::ROLE_CLIENT);
    ASSERT_FALSE(state.name.empty());
  }

  void PressEscape() {
    aura::Window* window = ash::wm::GetActiveWindow();
    CHECK(window);
    wait_for_key_press_.reset(new base::RunLoop());
    ui_controls::SendKeyPressNotifyWhenDone(
        window, ui::VKEY_ESCAPE,
        false, false, false, false,
        base::Bind(&AshFocusCycleTest::QuitWaiting, base::Unretained(this)));
    wait_for_key_press_->Run();
  }

 private:
  scoped_ptr<base::RunLoop> wait_for_key_press_;

  void QuitWaiting() {
    wait_for_key_press_->Quit();
  }
};

IN_PROC_BROWSER_TEST_F(AshFocusCycleTest, CycleFocus) {
  // Initially, the browser frame should have focus.
  ASSERT_STREQ("BrowserFrameAura", ash::wm::GetActiveWindow()->name().c_str());
  CheckFocusedViewIsAccessible();

  // Rotate focus, as if the user pressed Ctrl+Forward (Ctrl+F2).
  // The first item focused should be in the system tray.
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->RotateFocus(ash::Shell::FORWARD);
  ASSERT_STREQ("StatusAreaWidget", ash::wm::GetActiveWindow()->name().c_str());
  CheckFocusedViewIsAccessible();

  // Rotate focus again, now we should have something in the launcher focused.
  shell->RotateFocus(ash::Shell::FORWARD);
  ASSERT_STREQ("LauncherView", ash::wm::GetActiveWindow()->name().c_str());
  CheckFocusedViewIsAccessible();

  // Finally we should be back at the browser frame.
  shell->RotateFocus(ash::Shell::FORWARD);
  ASSERT_STREQ("BrowserFrameAura", ash::wm::GetActiveWindow()->name().c_str());
  CheckFocusedViewIsAccessible();
}

IN_PROC_BROWSER_TEST_F(AshFocusCycleTest, EscapeFromSystemTray) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->RotateFocus(ash::Shell::FORWARD);
  ASSERT_STREQ("StatusAreaWidget", ash::wm::GetActiveWindow()->name().c_str());

  PressEscape();
  ASSERT_STREQ("BrowserFrameAura", ash::wm::GetActiveWindow()->name().c_str());
  CheckFocusedViewIsAccessible();
}

IN_PROC_BROWSER_TEST_F(AshFocusCycleTest, EscapeFromLauncher) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->RotateFocus(ash::Shell::FORWARD);
  shell->RotateFocus(ash::Shell::FORWARD);
  ASSERT_STREQ("LauncherView", ash::wm::GetActiveWindow()->name().c_str());

  PressEscape();
  ASSERT_STREQ("BrowserFrameAura", ash::wm::GetActiveWindow()->name().c_str());
  CheckFocusedViewIsAccessible();
}

}  // namespace chromeos
