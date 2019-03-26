// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.test-mojom-test-utils.h"
#include "ash/public/interfaces/shell_test_api.test-mojom.h"
#include "ash/shell.h"                               // mash-ok
#include "ash/wm/splitview/split_view_controller.h"  // mash-ok
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

ws::Id GetBrowserWindowServerId(Browser* browser) {
  return aura::WindowMus::Get(
             browser->window()->GetNativeWindow()->GetRootWindow())
      ->server_id();
}

class SplitViewTest : public InProcessBrowserTest {
 public:
  SplitViewTest() = default;
  ~SplitViewTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make sure the test actually draws to screen and uses the real gpu.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
    command_line->AppendSwitch(switches::kUseGpuInTests);
    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SplitViewTest);
};

void WaitForNoPointerHoldLock() {
  ash::mojom::ShellTestApiPtr shell_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_test_api);
  ash::mojom::ShellTestApiAsyncWaiter waiter(shell_test_api.get());
  waiter.WaitForNoPointerHoldLock();
  aura::test::WaitForAllChangesToComplete();
}

// Used to wait for a window resize to show up on screen.
class WidgetResizeWaiter : public views::WidgetObserver {
 public:
  explicit WidgetResizeWaiter(views::Widget* widget) : widget_(widget) {
    widget_->AddObserver(this);
  }

  ~WidgetResizeWaiter() override {
    widget_->RemoveObserver(this);
    EXPECT_FALSE(waiting_for_frame_);
  }

  void WaitForDisplay() {
    if (waiting_for_frame_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      EXPECT_FALSE(waiting_for_frame_);
    }
    WaitForNoPointerHoldLock();
  }

 private:
  void OnFramePresented(const gfx::PresentationFeedback& feedback) {
    waiting_for_frame_ = false;
    if (run_loop_)
      run_loop_->Quit();
  }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    ui::Compositor* compositor =
        widget->GetNativeWindow()->GetHost()->compositor();
    ASSERT_TRUE(compositor);
    waiting_for_frame_ = true;
    compositor->RequestPresentationTimeForNextFrame(base::BindOnce(
        &WidgetResizeWaiter::OnFramePresented, base::Unretained(this)));
  }

  views::Widget* widget_;
  bool waiting_for_frame_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WidgetResizeWaiter);
};

IN_PROC_BROWSER_TEST_F(SplitViewTest, SplitViewResize) {
  // This test is intended to gauge performance of resizing windows while in
  // tablet mode split view. It does the following:
  // . creates two browser windows.
  // . enters tablet mode.
  // . snaps one to the left, one to the right.
  // . drags the resizer, which triggers resizing both browsers.
  browser()->window()->Maximize();

  Browser* browser2 = CreateBrowser(browser()->profile());
  browser2->window()->Show();
  browser2->window()->Maximize();

  // If running on device, wait a bit so that the display is actually powered
  // on.
  // TODO: this should be better factored.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                          base::TimeDelta::FromSeconds(5));
    run_loop.Run();
  }

  test::SetAndWaitForTabletMode(true);

  views::Widget* browser_widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::Widget* browser2_widget =
      BrowserView::GetBrowserViewForBrowser(browser2)->GetWidget();
  if (features::IsUsingWindowService()) {
    ash::mojom::ShellTestApiPtr shell_test_api;
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ash::mojom::kServiceName, &shell_test_api);

    {
      base::RunLoop run_loop;
      shell_test_api->SnapWindowInSplitView(content::mojom::kBrowserServiceName,
                                            GetBrowserWindowServerId(browser2),
                                            true, run_loop.QuitClosure());
      run_loop.Run();
    }

    {
      base::RunLoop run_loop;
      shell_test_api->SnapWindowInSplitView(content::mojom::kBrowserServiceName,
                                            GetBrowserWindowServerId(browser()),
                                            false, run_loop.QuitClosure());
      run_loop.Run();
    }
  } else {
    ash::Shell* shell = ash::Shell::Get();
    shell->split_view_controller()->SnapWindow(
        browser2_widget->GetNativeWindow(), ash::SplitViewController::LEFT);
    shell->split_view_controller()->FlushForTesting();
    shell->split_view_controller()->SnapWindow(
        browser_widget->GetNativeWindow(), ash::SplitViewController::RIGHT);
    shell->split_view_controller()->FlushForTesting();
  }

  WaitForNoPointerHoldLock();

  const gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().size();
  const gfx::Point start_position(display_size.width() / 2,
                                  display_size.height() / 2);
  ASSERT_TRUE(
      ui_test_utils::SendMouseMoveSync(
          gfx::Point(start_position.x(), start_position.y())) &&
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));

  constexpr int kNumIncrements = 20;
  int width = browser_widget->GetWindowBoundsInScreen().width();
  for (int i = 0; i < kNumIncrements; ++i) {
    const gfx::Point resize_point(start_position.x() - i - 1,
                                  start_position.y());
    WidgetResizeWaiter observer(browser_widget);
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(resize_point));
    observer.WaitForDisplay();
    ++width;
    EXPECT_EQ(width, browser_widget->GetWindowBoundsInScreen().width());
  }
}

}  // namespace
