// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/wm_window.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace {

class WindowSizerTest : public InProcessBrowserTest {
 public:
  WindowSizerTest() {}
  ~WindowSizerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "600x600,601+0-600x600");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowSizerTest);
};

void CloseBrowser(Browser* browser) {
  browser->window()->Close();
  base::RunLoop().RunUntilIdle();
}

gfx::Rect GetChromeIconBoundsForRootWindow(aura::Window* root_window) {
  const ash::ShelfView* shelf_view =
      ash::Shelf::ForWindow(root_window)->GetShelfViewForTesting();
  const views::ViewModel* view_model = shelf_view->view_model_for_test();

  EXPECT_EQ(2, view_model->view_size());
  return view_model->view_at(1)->GetBoundsInScreen();
}

void OpenBrowserUsingShelfOnRootWindow(aura::Window* root_window) {
  ui::test::EventGenerator generator(root_window);
  gfx::Point center =
      GetChromeIconBoundsForRootWindow(root_window).CenterPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  const gfx::Point& origin = display.bounds().origin();
  center.Offset(- origin.x(), - origin.y());
  generator.MoveMouseTo(center);
  generator.ClickLeftButton();
}

}  // namespace

#if !defined(OS_CHROMEOS)
#define MAYBE_OpenBrowserUsingShelfOnOtherDisplay \
  DISABLED_OpenBrowserUsingShelfOnOtherDisplay
#define MAYBE_OpenBrowserUsingContextMenuOnOtherDisplay \
  DISABLED_OpenBrowserUsingContextMenuOnOtherDisplay
#else
#define MAYBE_OpenBrowserUsingShelfOnOtherDisplay \
  OpenBrowserUsingShelfOnOtherDisplay
#define MAYBE_OpenBrowserUsingContextMenuOnOtherDisplay \
  OpenBrowserUsingContextMenuOnOtherDisplay
#endif

IN_PROC_BROWSER_TEST_F(WindowSizerTest,
                       MAYBE_OpenBrowserUsingShelfOnOtherDisplay) {
  // Don't shutdown when closing the last browser window.
  ScopedKeepAlive test_keep_alive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                                  KeepAliveRestartOption::DISABLED);

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  BrowserList* browser_list = BrowserList::GetInstance();

  EXPECT_EQ(1u, browser_list->size());
  // Close the browser window so that clicking icon will create a new window.
  CloseBrowser(browser_list->get(0));
  EXPECT_EQ(0u, browser_list->size());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());

  OpenBrowserUsingShelfOnRootWindow(root_windows[1]);

  // A new browser must be created on 2nd display.
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(root_windows[1],
            browser_list->get(0)->window()->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(root_windows[1], ash::Shell::GetRootWindowForNewWindows());

  // Close the browser window so that clicking icon will create a new window.
  CloseBrowser(browser_list->get(0));
  EXPECT_EQ(0u, browser_list->size());

  OpenBrowserUsingShelfOnRootWindow(root_windows[0]);

  // A new browser must be created on 1st display.
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(root_windows[0],
            browser_list->get(0)->window()->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());
}

namespace {

class WindowSizerContextMenuTest : public WindowSizerTest {
 public:
  WindowSizerContextMenuTest() {}
  ~WindowSizerContextMenuTest() override {}

  static void Step1(gfx::Point release_point) {
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::RIGHT, ui_controls::DOWN,
        base::Bind(&WindowSizerContextMenuTest::Step2, release_point));
  }

  static void Step2(gfx::Point release_point) {
    ui_controls::SendMouseMoveNotifyWhenDone(
        release_point.x(), release_point.y(),
        base::Bind(&WindowSizerContextMenuTest::Step3));
  }

  static void Step3() {
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::RIGHT, ui_controls::UP,
        base::Bind(&WindowSizerContextMenuTest::QuitLoop));
  }

  static void QuitLoop() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowSizerContextMenuTest);
};

void OpenBrowserUsingContextMenuOnRootWindow(aura::Window* root_window) {
  gfx::Point chrome_icon =
      GetChromeIconBoundsForRootWindow(root_window).CenterPoint();
  gfx::Point release_point = chrome_icon;
  // -153 moves the cursor up to the "New window" menu option.
  release_point.Offset(0, -153);
  ui_controls::SendMouseMoveNotifyWhenDone(
      chrome_icon.x(), chrome_icon.y(),
      base::Bind(&WindowSizerContextMenuTest::Step1, release_point));
  base::RunLoop().Run();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(WindowSizerContextMenuTest,
                       MAYBE_OpenBrowserUsingContextMenuOnOtherDisplay) {
  // Don't shutdown when closing the last browser window.
  ScopedKeepAlive test_keep_alive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                                  KeepAliveRestartOption::DISABLED);

  views::MenuController::TurnOffMenuSelectionHoldForTest();

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  BrowserList* browser_list = BrowserList::GetInstance();

  ASSERT_EQ(1u, browser_list->size());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());
  CloseBrowser(browser_list->get(0));

  OpenBrowserUsingContextMenuOnRootWindow(root_windows[1]);

  // A new browser must be created on 2nd display.
  ASSERT_EQ(1u, browser_list->size());
  EXPECT_EQ(root_windows[1],
            browser_list->get(0)->window()->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(root_windows[1], ash::Shell::GetRootWindowForNewWindows());

  CloseBrowser(browser_list->get(0));
  OpenBrowserUsingContextMenuOnRootWindow(root_windows[0]);

  // Next new browser must be created on 1st display.
  ASSERT_EQ(1u, browser_list->size());
  EXPECT_EQ(root_windows[0],
            browser_list->get(0)->window()->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());
}
