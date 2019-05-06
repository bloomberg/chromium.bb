// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/perf/performance_test.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/core/wm_core_switches.h"

namespace {

class TestLayerAnimationObserver : public ui::LayerAnimationObserver {
 public:
  explicit TestLayerAnimationObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}
  ~TestLayerAnimationObserver() override = default;

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    if (!callback_.is_null())
      std::move(callback_).Run();
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  base::OnceClosure callback_;
  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimationObserver);
};

}  // namespace

class TabletModeTransitionTest : public UIPerformanceTest {
 public:
  TabletModeTransitionTest() = default;
  ~TabletModeTransitionTest() override = default;

  // UIPerformanceTest:
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();

    // The uma stats we are interested in measure the animations directly so we
    // need to ensure they are turned on.
    auto* cmd = base::CommandLine::ForCurrentProcess();
    if (cmd->HasSwitch(wm::switches::kWindowAnimationsDisabled))
      cmd->RemoveSwitch(wm::switches::kWindowAnimationsDisabled);

    constexpr int additional_browsers = 5;
    constexpr float cost_per_browser = 0.1;
    for (int i = 0; i < additional_browsers; ++i)
      CreateBrowser(browser()->profile());

    int wait_seconds = (base::SysInfo::IsRunningOnChromeOS() ? 5 : 0) +
                       (additional_browsers + 1) * cost_per_browser;
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                          base::TimeDelta::FromSeconds(wait_seconds));
    run_loop.Run();
  }

  std::vector<std::string> GetUMAHistogramNames() const override {
    return {
        "Ash.TabletMode.AnimationSmoothness.Enter",
        "Ash.TabletMode.AnimationSmoothness.Exit",
    };
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabletModeTransitionTest);
};

IN_PROC_BROWSER_TEST_F(TabletModeTransitionTest, EnterExit) {
  // Activate the first window. The top window is the only window which animates
  // and is the one we should check to see if the tablet animation has finished.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  ::wm::ActivateWindow(browser_window);

  {
    base::RunLoop run_loop;
    TestLayerAnimationObserver waiter(run_loop.QuitClosure());
    browser_window->layer()->GetAnimator()->AddObserver(&waiter);
    test::SetAndWaitForTabletMode(true);
    run_loop.Run();
    browser_window->layer()->GetAnimator()->RemoveObserver(&waiter);
  }

  {
    base::RunLoop run_loop;
    TestLayerAnimationObserver waiter(run_loop.QuitClosure());
    browser_window->layer()->GetAnimator()->AddObserver(&waiter);
    test::SetAndWaitForTabletMode(false);
    run_loop.Run();
    browser_window->layer()->GetAnimator()->RemoveObserver(&waiter);
  }
}
