// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/protocol/browser_handler.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/test/base/in_process_browser_test.h"

#if defined(OS_MACOSX)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

// Encapsulates waiting for the browser window to change state. This is
// needed for example on Chrome desktop linux, where window state change is done
// asynchronously as an event received from a different process.
class CheckWaiter {
 public:
  explicit CheckWaiter(base::Callback<bool()> callback, bool expected)
      : callback_(callback),
        expected_(expected),
        timeout_(base::Time::NowFromSystemTime() +
                 base::TimeDelta::FromSeconds(1)) {}
  ~CheckWaiter() = default;

  // Blocks until the browser window becomes maximized.
  void Wait() {
    if (Check())
      return;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  bool Check() {
    if (callback_.Run() != expected_ &&
        base::Time::NowFromSystemTime() < timeout_) {
      base::MessageLoop::current()->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(base::IgnoreResult(&CheckWaiter::Check),
                                    base::Unretained(this)));
      return false;
    }

    // Quit the run_loop to end the wait.
    if (!quit_.is_null())
      base::ResetAndReturn(&quit_).Run();
    return true;
  }

  base::Callback<bool()> callback_;
  bool expected_;
  base::Time timeout_;
  // The waiter's RunLoop quit closure.
  base::Closure quit_;

  DISALLOW_COPY_AND_ASSIGN(CheckWaiter);
};

class DevToolsManagerDelegateTest : public InProcessBrowserTest {
 public:
  void SendCommand(std::string state) {
    auto window_bounds =
        protocol::Browser::Bounds::Create().SetWindowState(state).Build();
    BrowserHandler handler(nullptr);
    handler.SetWindowBounds(browser()->session_id().id(),
                            std::move(window_bounds));
  }

  void UpdateBounds() {
    auto window_bounds = protocol::Browser::Bounds::Create()
                             .SetWindowState("normal")
                             .SetLeft(200)
                             .SetHeight(400)
                             .Build();
    BrowserHandler handler(nullptr);
    handler.SetWindowBounds(browser()->session_id().id(),
                            std::move(window_bounds));
  }

  void CheckIsMaximized(bool maximized) {
    CheckWaiter(base::Bind(&BrowserWindow::IsMaximized,
                           base::Unretained(browser()->window())),
                maximized)
        .Wait();
    EXPECT_EQ(maximized, browser()->window()->IsMaximized());
  }

  void CheckIsMinimized(bool minimized) {
    CheckWaiter(base::Bind(&BrowserWindow::IsMinimized,
                           base::Unretained(browser()->window())),
                minimized)
        .Wait();
    EXPECT_EQ(minimized, browser()->window()->IsMinimized());
  }

  void CheckIsFullscreen(bool fullscreen) {
    CheckWaiter(base::Bind(&BrowserWindow::IsFullscreen,
                           base::Unretained(browser()->window())),
                fullscreen)
        .Wait();
    EXPECT_EQ(fullscreen, browser()->window()->IsFullscreen());
  }

  bool IsWindowBoundsEqual(gfx::Rect expected) {
    return browser()->window()->GetBounds() == expected;
  }

  void CheckWindowBounds(gfx::Rect expected) {
    CheckWaiter(base::Bind(&DevToolsManagerDelegateTest::IsWindowBoundsEqual,
                           base::Unretained(this), expected),
                true)
        .Wait();
    EXPECT_EQ(expected, browser()->window()->GetBounds());
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalWindowChangeBounds) {
  browser()->window()->SetBounds(gfx::Rect(100, 100, 500, 600));
  CheckWindowBounds(gfx::Rect(100, 100, 500, 600));
  UpdateBounds();
  CheckWindowBounds(gfx::Rect(200, 100, 500, 400));
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalToMaximizedWindow) {
  CheckIsMaximized(false);
  SendCommand("maximized");
  CheckIsMaximized(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalToMinimizedWindow) {
  CheckIsMinimized(false);
  SendCommand("minimized");
  CheckIsMinimized(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, NormalToFullscreenWindow) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen faker;
#endif
  CheckIsFullscreen(false);
  SendCommand("fullscreen");
#if defined(OS_MACOSX)
  faker.FinishTransition();
#endif
  CheckIsFullscreen(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MaximizedToMinimizedWindow) {
  browser()->window()->Maximize();
  CheckIsMaximized(true);

  CheckIsMinimized(false);
  SendCommand("minimized");
  CheckIsMinimized(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest,
                       MaximizedToFullscreenWindow) {
  browser()->window()->Maximize();
  CheckIsMaximized(true);

#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen faker;
#endif
  CheckIsFullscreen(false);
  SendCommand("fullscreen");
#if defined(OS_MACOSX)
  faker.FinishTransition();
#endif
  CheckIsFullscreen(true);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, ShowMinimizedWindow) {
  browser()->window()->Minimize();
  CheckIsMinimized(true);
  SendCommand("normal");
  CheckIsMinimized(false);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, RestoreMaximizedWindow) {
  browser()->window()->Maximize();
  CheckIsMaximized(true);
  SendCommand("normal");
  CheckIsMaximized(false);
}

IN_PROC_BROWSER_TEST_F(DevToolsManagerDelegateTest, ExitFullscreenWindow) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen faker;
#endif
  browser()->window()->GetExclusiveAccessContext()->EnterFullscreen(
      GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
#if defined(OS_MACOSX)
  faker.FinishTransition();
#endif
  CheckIsFullscreen(true);
  SendCommand("normal");
#if defined(OS_MACOSX)
  faker.FinishTransition();
#endif
  CheckIsFullscreen(false);
}
