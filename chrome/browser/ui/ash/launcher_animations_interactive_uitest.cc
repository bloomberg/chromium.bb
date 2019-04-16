// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/interfaces/app_list_view.mojom.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/performance_test.h"
#include "content/public/test/test_utils.h"
#include "ui/base/test/ui_controls.h"

// TODO(oshima): Add tablet mode overview transition.
class LauncherAnimationsTest : public UIPerformanceTest {
 public:
  LauncherAnimationsTest() = default;
  ~LauncherAnimationsTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Apps.StateTransition.AnimationSmoothness.TabletMode",
    };
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsTest);
};

IN_PROC_BROWSER_TEST_F(LauncherAnimationsTest, Fullscreen) {
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::mojom::ShellTestApiPtr shell_test_api = test::GetShellTestApi();
  {
    base::RunLoop waiter;
    shell_test_api->WaitForLauncherAnimationState(
        ash::mojom::AppListViewState::kFullscreenAllApps, waiter.QuitClosure());
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/true,
                              /*alt=*/false,
                              /* command = */ false);
    waiter.Run();
  }
  {
    base::RunLoop waiter;
    shell_test_api->WaitForLauncherAnimationState(
        ash::mojom::AppListViewState::kClosed, waiter.QuitClosure());
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/true,
                              /*alt=*/false,
                              /* command = */ false);

    waiter.Run();
  }
}

IN_PROC_BROWSER_TEST_F(LauncherAnimationsTest, Peeking) {
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::mojom::ShellTestApiPtr shell_test_api = test::GetShellTestApi();
  {
    base::RunLoop waiter;
    shell_test_api->WaitForLauncherAnimationState(
        ash::mojom::AppListViewState::kPeeking, waiter.QuitClosure());
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/false,
                              /*alt=*/false,
                              /* command = */ false);
    waiter.Run();
  }
  {
    base::RunLoop waiter;
    shell_test_api->WaitForLauncherAnimationState(
        ash::mojom::AppListViewState::kClosed, waiter.QuitClosure());
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/false,
                              /*alt=*/false,
                              /* command = */ false);
    waiter.Run();
  }
}
