// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/interfaces/app_list_view.mojom.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
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

    test::PopulateDummyAppListItems(100);
    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    DCHECK(!suffix_.empty());
    return {
        std::string("Apps.StateTransition.AnimationSmoothness.") + suffix_,
        "Apps.StateTransition.AnimationSmoothness.Close.ClamshellMode",
    };
  }

 protected:
  void set_suffix(const std::string& suffix) { suffix_ = suffix; }

 private:
  std::string suffix_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsTest);
};

IN_PROC_BROWSER_TEST_F(LauncherAnimationsTest, Fullscreen) {
  set_suffix("FullscreenAllApps.ClamshellMode");
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
  set_suffix("Peeking.ClamshellMode");
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

IN_PROC_BROWSER_TEST_F(LauncherAnimationsTest, Half) {
  set_suffix("Half.ClamshellMode");
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::mojom::ShellTestApiPtr shell_test_api = test::GetShellTestApi();
  {
    // Hit the search key; it should switch to kPeeking state.
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
    // Type some query in the launcher; it should show search results in kHalf
    // state.
    base::RunLoop waiter;
    shell_test_api->WaitForLauncherAnimationState(
        ash::mojom::AppListViewState::kHalf, waiter.QuitClosure());
    ui_controls::SendKeyPress(browser_window, ui::VKEY_A,
                              /*control=*/false,
                              /*shift=*/false,
                              /*alt=*/false,
                              /* command = */ false);
    waiter.Run();
  }
  {
    // Search key to close the launcher.
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

IN_PROC_BROWSER_TEST_F(LauncherAnimationsTest, FullscreenSearch) {
  set_suffix("FullscreenSearch.ClamshellMode");
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::mojom::ShellTestApiPtr shell_test_api = test::GetShellTestApi();
  {
    // Hit the search key; it should switch to the kPeeking state.
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
    // Type some query; it should show the search results in the kHalf state.
    base::RunLoop waiter;
    shell_test_api->WaitForLauncherAnimationState(
        ash::mojom::AppListViewState::kHalf, waiter.QuitClosure());
    ui_controls::SendKeyPress(browser_window, ui::VKEY_A,
                              /*control=*/false,
                              /*shift=*/false,
                              /*alt=*/false,
                              /* command = */ false);
    waiter.Run();
  }
  {
    // Shift+search key; it should expand to fullscreen with search results
    // (i.e. kFullscreenSearch state).
    base::RunLoop waiter;
    shell_test_api->WaitForLauncherAnimationState(
        ash::mojom::AppListViewState::kFullscreenSearch, waiter.QuitClosure());
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/true,
                              /*alt=*/false,
                              /* command = */ false);
    waiter.Run();
  }
  {
    // Search key to close the launcher.
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
