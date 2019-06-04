// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/performance_test.h"
#include "content/public/test/test_utils.h"
#include "ui/base/test/ui_controls.h"

// TODO(oshima): Add tablet mode overview transition.
class LauncherAnimationsTest : public UIPerformanceTest,
                               public ::testing::WithParamInterface<bool> {
 public:
  LauncherAnimationsTest() = default;
  ~LauncherAnimationsTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();
    reuse_widget_ = GetParam();

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

  // Create the cached widget of the app-list prior to the actual test scenario.
  void MaybeCreateCachedWidget() {
    if (!reuse_widget_)
      return;

    // Here goes through several states of the app-list so that the cached
    // widget can contain relevant data.
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
    ash::ShellTestApi shell_test_api;
    // Open the app-list with peeking state.
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/false,
                              /*alt=*/false,
                              /* command = */ false);
    shell_test_api.WaitForLauncherAnimationState(
        ash::AppListViewState::kPeeking);

    // Expand to the fullscreen with list of applications.
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/true,
                              /*alt=*/false,
                              /* command = */ false);
    shell_test_api.WaitForLauncherAnimationState(
        ash::AppListViewState::kFullscreenAllApps);

    // Type a random query to switch to search result view.
    ui_controls::SendKeyPress(browser_window, ui::VKEY_X,
                              /*control=*/false,
                              /*shift=*/false,
                              /*alt=*/false,
                              /* command = */ false);
    shell_test_api.WaitForLauncherAnimationState(
        ash::AppListViewState::kFullscreenSearch);

    // Close.
    ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                              /*control=*/false,
                              /*shift=*/false,
                              /*alt=*/false,
                              /* command = */ false);
    shell_test_api.WaitForLauncherAnimationState(
        ash::AppListViewState::kClosed);

    // Takes the snapshot delta; so that the samples created so far will be
    // eliminated from the samples.
    for (const auto& name : GetUMAHistogramNames()) {
      auto* histogram = base::StatisticsRecorder::FindHistogram(name);
      if (!histogram)
        continue;
      histogram->SnapshotDelta();
    }
  }

 private:
  bool reuse_widget_ = false;
  std::string suffix_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAnimationsTest);
};

IN_PROC_BROWSER_TEST_P(LauncherAnimationsTest, Fullscreen) {
  set_suffix("FullscreenAllApps.ClamshellMode");
  MaybeCreateCachedWidget();
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::ShellTestApi shell_test_api;
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/true,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(
      ash::AppListViewState::kFullscreenAllApps);

  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/true,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kClosed);
}

IN_PROC_BROWSER_TEST_P(LauncherAnimationsTest, Peeking) {
  set_suffix("Peeking.ClamshellMode");
  MaybeCreateCachedWidget();
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::ShellTestApi shell_test_api;
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kPeeking);

  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kClosed);
}

IN_PROC_BROWSER_TEST_P(LauncherAnimationsTest, Half) {
  set_suffix("Half.ClamshellMode");
  MaybeCreateCachedWidget();
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::ShellTestApi shell_test_api;
  // Hit the search key; it should switch to kPeeking state.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);

  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kPeeking);

  // Type some query in the launcher; it should show search results in kHalf
  // state.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_A,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kHalf);

  // Search key to close the launcher.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kClosed);
}

IN_PROC_BROWSER_TEST_P(LauncherAnimationsTest, FullscreenSearch) {
  set_suffix("FullscreenSearch.ClamshellMode");
  MaybeCreateCachedWidget();
  // Browser window is used to identify display, so we can use
  // use the 1st browser window regardless of number of windows created.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ash::ShellTestApi shell_test_api;
  // Hit the search key; it should switch to the kPeeking state.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kPeeking);

  // Type some query; it should show the search results in the kHalf state.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_A,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kHalf);

  // Shift+search key; it should expand to fullscreen with search results
  // (i.e. kFullscreenSearch state).
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/true,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(
      ash::AppListViewState::kFullscreenSearch);

  // Search key to close the launcher.
  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_SEARCH,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /* command = */ false);
  shell_test_api.WaitForLauncherAnimationState(ash::AppListViewState::kClosed);
}

INSTANTIATE_TEST_SUITE_P(,
                         LauncherAnimationsTest,
                         /*reuse_widget=*/::testing::Bool());
