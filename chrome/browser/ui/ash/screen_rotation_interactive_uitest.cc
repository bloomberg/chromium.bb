// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/performance_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer_animation_observer.h"

namespace {

// Test overview enter/exit animations with following conditions
// int: number of windows : 2, 8
// bool: the tab content (chrome://blank, chrome://newtab)
class ScreenRotationTest
    : public UIPerformanceTest,
      public testing::WithParamInterface<::testing::tuple<int, bool>> {
 public:
  ScreenRotationTest() = default;
  ~ScreenRotationTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);
    auto* pref = browser()->profile()->GetPrefs();
    pref->SetBoolean(
        ash::prefs::kDisplayRotationAcceleratorDialogHasBeenAccepted2, true);

    int additional_browsers = std::get<0>(GetParam()) - 1;
    ntp_page_ = std::get<1>(GetParam());

    GURL ntp_url("chrome://newtab");
    // The default is blank page.
    if (ntp_page_)
      ui_test_utils::NavigateToURL(browser(), ntp_url);

    for (int i = additional_browsers; i > 0; i--) {
      Browser* new_browser = CreateBrowser(browser()->profile());
      if (ntp_page_)
        ui_test_utils::NavigateToURL(new_browser, ntp_url);
    }

    float cost_per_browser = ntp_page_ ? 0.5 : 0.1;
    int wait_seconds = (base::SysInfo::IsRunningOnChromeOS() ? 5 : 0) +
                       additional_browsers * cost_per_browser;
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromSeconds(wait_seconds));
    run_loop.Run();
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    return {"Ash.Rotation.AnimationSmoothness"};
  }

  bool ntp_page() const { return ntp_page_; }

 private:
  bool ntp_page_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationTest);
};

class ScreenRotationWaiter : public ash::ScreenRotationAnimatorObserver {
 public:
  explicit ScreenRotationWaiter(aura::Window* root_window)
      : animator_(ash::ScreenRotationAnimator::GetForRootWindow(root_window)) {
    animator_->AddObserver(this);
  }
  ~ScreenRotationWaiter() override { animator_->RemoveObserver(this); }

  // ash::ScreenRotationAnimationObserver:
  void OnScreenCopiedBeforeRotation() override {}
  void OnScreenRotationAnimationFinished(ash::ScreenRotationAnimator* animator,
                                         bool canceled) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  ash::ScreenRotationAnimator* animator_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationWaiter);
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ScreenRotationTest, RotateInTablet) {
  // Browser window is used just to identify display.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  gfx::NativeWindow browser_window =
      browser_view->GetWidget()->GetNativeWindow();

  ScreenRotationWaiter waiter(browser_window->GetRootWindow());

  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_REFRESH,
                            /*control=*/true,
                            /*shift=*/true,
                            /*alt=*/false,
                            /*command=*/false);
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(ScreenRotationTest, RotateInTabletOverview) {
  // Browser window is used just to identify display.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  gfx::NativeWindow browser_window =
      browser_view->GetWidget()->GetNativeWindow();
  ui_controls::SendKeyPress(browser_window, ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);
  // Wait until the window labels are shown.
  {
    auto windows = ash::ShellTestApi().GetItemWindowListInOverviewGrids();
    ASSERT_TRUE(windows.size() > 0);
    // Skipping the wait if the animation is not ongoing because it could get
    // stuck if the window animation has already finished at this point. There
    // might be a chance that window animation hasn't started yet, and if so
    // the test can't measure the right performance, but not failing.
    // TODO(mukai): Find the way to check if the animation has finished or not,
    // and replace the waiter by CreateWaiterForFinishingWindowAnimation().
    if (windows[0]->layer()->GetAnimator()->is_animating())
      ash::ShellTestApi().WaitForWindowFinishAnimating(windows[0]);
  }

  ScreenRotationWaiter waiter(browser_window->GetRootWindow());

  ui_controls::SendKeyPress(browser_window, ui::VKEY_BROWSER_REFRESH,
                            /*control=*/true,
                            /*shift=*/true,
                            /*alt=*/false,
                            /*command=*/false);
  waiter.Wait();
}

// TODO(oshima): Support split screen in tablet mode.
// TODO(oshima): Support overview mode.

INSTANTIATE_TEST_SUITE_P(All,
                         ScreenRotationTest,
                         ::testing::Combine(::testing::Values(2, 8),
                                            /*blank=*/testing::Bool()));
