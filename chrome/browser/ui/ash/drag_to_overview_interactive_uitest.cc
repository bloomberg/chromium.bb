// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/ash/ash_test_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/perf/performance_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

namespace {

bool IsOverviewSelecting() {
  ash::mojom::ShellTestApiPtr shell_test_api = test::GetShellTestApi();
  ash::mojom::ShellTestApiAsyncWaiter waiter(shell_test_api.get());
  bool is_selecting = false;
  waiter.IsOverviewSelecting(&is_selecting);
  return is_selecting;
}

int GetDetachY(TabStrip* tab_strip) {
  return std::max(TabDragController::kTouchVerticalDetachMagnetism,
                  TabDragController::kVerticalDetachMagnetism) +
         tab_strip->height() + 1;
}

// Waits for the primary display to present a frame after the object is
// constructed.
class NextFrameWaiter {
 public:
  NextFrameWaiter() : shell_test_api_(test::GetShellTestApi()) {
    shell_test_api_->WaitForNextFrame(base::BindOnce(
        &NextFrameWaiter::OnFramePresented, base::Unretained(this)));
    // Flush to ensure the call has gone through.
    shell_test_api_.FlushForTesting();
  }
  ~NextFrameWaiter() { EXPECT_TRUE(frame_presented_); }

  void WaitForDisplay() {
    if (!frame_presented_) {
      run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
      run_loop_->Run();
      EXPECT_TRUE(frame_presented_);
    }
  }

 private:
  void OnFramePresented() {
    frame_presented_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  ash::mojom::ShellTestApiPtr shell_test_api_;
  bool frame_presented_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NextFrameWaiter);
};

}  // namespace

class DragToOverviewTest : public UIPerformanceTest {
 public:
  DragToOverviewTest() = default;
  ~DragToOverviewTest() override = default;

  // UIPerformanceTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }
  void SetUpOnMainThread() override {
    UIPerformanceTest::SetUpOnMainThread();
    test::SetAndWaitForTabletMode(true);

    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::RunLoop run_loop;
      base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                            base::TimeDelta::FromSeconds(5));
      run_loop.Run();
    }
  }
  std::vector<std::string> GetUMAHistogramNames() const override {
    if (tab_drag_test_) {
      return {"Ash.SwipeDownDrag.Tab.PresentationTime.TabletMode",
              "Ash.SwipeDownDrag.Tab.PresentationTime.MaxLatency.TabletMode"};
    }
    return {
        "Ash.SwipeDownDrag.Window.PresentationTime.TabletMode",
        "Ash.SwipeDownDrag.Window.PresentationTime.MaxLatency.TabletMode",
    };
  }

  // Continue a drag by perform |count| steps mouse dragging with each step move
  // |delta|, then moue up.
  void ContinueDrag(const gfx::Point& start_position,
                    const gfx::Vector2d& delta,
                    int count) {
    gfx::Point drag_position = start_position;
    for (int i = 0; i < count; ++i) {
      drag_position += delta;

      test::WaitForNoPointerHoldLock(/*wait_for_changes=*/false);
      NextFrameWaiter waiter;
      ASSERT_TRUE(
          ui_controls::SendMouseMove(drag_position.x(), drag_position.y()));
      waiter.WaitForDisplay();
    }

    {
      NextFrameWaiter waiter;
      ASSERT_TRUE(
          ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP));
      waiter.WaitForDisplay();
    }
  }

  void VerifyTabDetachedAndContinueDrag(const gfx::Point& start_position,
                                        const gfx::Vector2d delta,
                                        int count) {
    // Tab should be detached to create a new browser window.
    EXPECT_EQ(2u, BrowserList::GetInstance()->size());
    EXPECT_TRUE(TabDragController::IsActive());

    ContinueDrag(start_position, delta, count);
  }

  void set_tab_drag_test(bool tab_drag_test) { tab_drag_test_ = tab_drag_test; }

 private:
  bool tab_drag_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(DragToOverviewTest);
};

IN_PROC_BROWSER_TEST_F(DragToOverviewTest, DragWindow) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();
  gfx::Rect browser_screen_bounds = browser_window->GetBoundsInScreen();

  const gfx::Point start_position(
      browser_screen_bounds.CenterPoint().x(),
      browser_screen_bounds.y() + browser_view->GetTabStripHeight() / 2);

  test::WaitForNoPointerHoldLock();
  ASSERT_TRUE(
      ui_test_utils::SendMouseMoveSync(start_position) &&
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));

  // One more mouse move to start drag.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(start_position));

  // Drag 25% of the work area height.
  const int drag_length = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(browser_window)
                              .work_area_size()
                              .height() /
                          4;
  constexpr int kSteps = 20;
  gfx::Vector2d delta(0, drag_length / kSteps);
  ContinueDrag(start_position, delta, kSteps);
  EXPECT_TRUE(IsOverviewSelecting());
}

IN_PROC_BROWSER_TEST_F(DragToOverviewTest, DragTab) {
  set_tab_drag_test(true);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  aura::Window* browser_window = browser_view->GetWidget()->GetNativeWindow();

  AddBlankTabAndShow(browser());
  browser_view->tabstrip()->StopAnimating(true);

  gfx::Point drag_position(ui_test_utils::GetCenterInScreenCoordinates(
      browser_view->tabstrip()->tab_at(0)));

  test::WaitForNoPointerHoldLock();
  ASSERT_TRUE(
      ui_test_utils::SendMouseMoveSync(drag_position) &&
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));

  // Drag 25% of the work area height.
  const int drag_length = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(browser_window)
                              .work_area_size()
                              .height() /
                          4;
  constexpr int kSteps = 20;
  gfx::Vector2d delta(0, drag_length / kSteps);

  // Drag tab far enough to detach.
  test::WaitForNoPointerHoldLock();
  drag_position.Offset(0, GetDetachY(browser_view->tabstrip()));
  ui_controls::SendMouseMoveNotifyWhenDone(
      drag_position.x(), drag_position.y(),
      base::BindOnce(&DragToOverviewTest::VerifyTabDetachedAndContinueDrag,
                     base::Unretained(this), drag_position, delta, kSteps));

  // Wait for the drag to finish.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
      content::NotificationService::AllSources())
      .Wait();
}
