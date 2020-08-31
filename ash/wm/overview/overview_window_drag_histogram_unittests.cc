// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/splitview/multi_display_overview_and_split_view_test.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class OverviewWindowDragHistogramTest
    : public MultiDisplayOverviewAndSplitViewTest {
 public:
  OverviewWindowDragHistogramTest() = default;
  ~OverviewWindowDragHistogramTest() override = default;
  OverviewWindowDragHistogramTest(const OverviewWindowDragHistogramTest&) =
      delete;
  OverviewWindowDragHistogramTest& operator=(
      const OverviewWindowDragHistogramTest&) = delete;

  void SetUp() override {
    // Always enable |features::kDragToSnapInClamshellMode|. When the test
    // parameter value is true, |features::kMultiDisplayOverviewAndSplitView| is
    // enabled too (see |MultiDisplayOverviewAndSplitViewTest|).
    scoped_feature_list_.InitAndEnableFeature(
        features::kDragToSnapInClamshellMode);
    MultiDisplayOverviewAndSplitViewTest::SetUp();
    window_ = CreateAppWindow();
  }

  void TearDown() override {
    window_.reset();
    MultiDisplayOverviewAndSplitViewTest::TearDown();
  }

 protected:
  void EnterTablet() {
    TabletModeControllerTestApi().DetachAllMice();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  }

  gfx::Point EnterOverviewAndGetItemCenterPoint() {
    Shell::Get()->overview_controller()->StartOverview();
    return gfx::ToRoundedPoint(
        GetOverviewItemForWindow(window_.get())->target_bounds().CenterPoint());
  }

  gfx::Point GetSecondDeskMiniViewCenterPoint(size_t grid_index) {
    return GetOverviewSession()
        ->grid_list()[grid_index]
        ->desks_bar_view()
        ->mini_views()[1u]
        ->GetBoundsInScreen()
        .CenterPoint();
  }

  void EnterTabletAndOverviewAndLongPressItem() {
    // Temporarily reconfigure gestures so the long press takes 2 milliseconds.
    ui::GestureConfiguration* gesture_config =
        ui::GestureConfiguration::GetInstance();
    const int old_long_press_time_in_ms =
        gesture_config->long_press_time_in_ms();
    const int old_show_press_delay_in_ms =
        gesture_config->show_press_delay_in_ms();
    gesture_config->set_long_press_time_in_ms(1);
    gesture_config->set_show_press_delay_in_ms(1);

    EnterTablet();
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_screen_location(
        EnterOverviewAndGetItemCenterPoint());
    generator->PressTouch();
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(2));
    run_loop.Run();

    gesture_config->set_long_press_time_in_ms(old_long_press_time_in_ms);
    gesture_config->set_show_press_delay_in_ms(old_show_press_delay_in_ms);
  }

  void EnterOverviewAndSwipeItemDown(unsigned int distance) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_screen_location(
        EnterOverviewAndGetItemCenterPoint());
    generator->PressTouch();
    // Use small increments to avoid flinging.
    for (unsigned int i = 0u; i < distance; ++i)
      generator->MoveTouchBy(0, 1);
    generator->ReleaseTouch();
  }

  void EnterOverviewAndFlingItemDown() {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_screen_location(
        EnterOverviewAndGetItemCenterPoint());
    generator->PressMoveAndReleaseTouchBy(0, 20);
  }

  void ExpectDragRecorded(OverviewDragAction action) {
    histogram_tester_.ExpectUniqueSample("Ash.Overview.WindowDrag.Workflow",
                                         action, 1);
  }

  void ExpectNoDragRecorded() {
    histogram_tester_.ExpectTotalCount("Ash.Overview.WindowDrag.Workflow", 0);
  }

 private:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<aura::Window> window_;
};

TEST_P(OverviewWindowDragHistogramTest, ToGridSameDisplayClamshellMouse) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseBy(5, 0);
  ExpectDragRecorded(OverviewDragAction::kToGridSameDisplayClamshellMouse);
}

TEST_P(OverviewWindowDragHistogramTest, ToGridSameDisplayClamshellTouch) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(EnterOverviewAndGetItemCenterPoint());
  generator->PressMoveAndReleaseTouchBy(20, 0);
  ExpectDragRecorded(OverviewDragAction::kToGridSameDisplayClamshellTouch);
}

TEST_P(OverviewWindowDragHistogramTest, ToDeskSameDisplayClamshellMouse) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  ExpectDragRecorded(OverviewDragAction::kToDeskSameDisplayClamshellMouse);
}

TEST_P(OverviewWindowDragHistogramTest, ToDeskSameDisplayClamshellTouch) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(EnterOverviewAndGetItemCenterPoint());
  generator->PressMoveAndReleaseTouchTo(
      GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  ExpectDragRecorded(OverviewDragAction::kToDeskSameDisplayClamshellTouch);
}

TEST_P(OverviewWindowDragHistogramTest, ToSnapSameDisplayClamshellMouse) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(0, 300);
  ExpectDragRecorded(OverviewDragAction::kToSnapSameDisplayClamshellMouse);
}

TEST_P(OverviewWindowDragHistogramTest, ToSnapSameDisplayClamshellTouch) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(EnterOverviewAndGetItemCenterPoint());
  generator->PressMoveAndReleaseTouchTo(0, 300);
  ExpectDragRecorded(OverviewDragAction::kToSnapSameDisplayClamshellTouch);
}

TEST_P(OverviewWindowDragHistogramTest, SwipeToCloseSuccessfulClamshellTouch) {
  EnterOverviewAndSwipeItemDown(180u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseSuccessfulClamshellTouch);
}

TEST_P(OverviewWindowDragHistogramTest, SwipeToCloseCanceledClamshellTouch) {
  EnterOverviewAndSwipeItemDown(20u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseCanceledClamshellTouch);
}

TEST_P(OverviewWindowDragHistogramTest, SwipeToCloseSuccessfulTabletTouch) {
  EnterTablet();
  EnterOverviewAndSwipeItemDown(180u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseSuccessfulTabletTouch);
}

TEST_P(OverviewWindowDragHistogramTest, SwipeToCloseCanceledTabletTouch) {
  EnterTablet();
  EnterOverviewAndSwipeItemDown(20u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseCanceledTabletTouch);
}

TEST_P(OverviewWindowDragHistogramTest, FlingToCloseClamshellTouch) {
  EnterOverviewAndFlingItemDown();
  ExpectDragRecorded(OverviewDragAction::kFlingToCloseClamshellTouch);
}

TEST_P(OverviewWindowDragHistogramTest, FlingToCloseTabletTouch) {
  EnterTablet();
  EnterOverviewAndFlingItemDown();
  ExpectDragRecorded(OverviewDragAction::kFlingToCloseTabletTouch);
}

TEST_P(OverviewWindowDragHistogramTest, ToGridSameDisplayTabletTouch) {
  EnterTabletAndOverviewAndLongPressItem();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouchBy(20, 0);
  generator->ReleaseTouch();
  ExpectDragRecorded(OverviewDragAction::kToGridSameDisplayTabletTouch);
}

TEST_P(OverviewWindowDragHistogramTest, ToDeskSameDisplayTabletTouch) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  EnterTabletAndOverviewAndLongPressItem();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  generator->ReleaseTouch();
  ExpectDragRecorded(OverviewDragAction::kToDeskSameDisplayTabletTouch);
}

TEST_P(OverviewWindowDragHistogramTest, ToSnapSameDisplayTabletTouch) {
  EnterTabletAndOverviewAndLongPressItem();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(gfx::Point(0, 300));
  generator->ReleaseTouch();
  ExpectDragRecorded(OverviewDragAction::kToSnapSameDisplayTabletTouch);
}

TEST_P(OverviewWindowDragHistogramTest, ToGridSameDisplayTabletMouse) {
  EnterTablet();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseBy(20, 0);
  ExpectNoDragRecorded();
}

TEST_P(OverviewWindowDragHistogramTest, ToDeskSameDisplayTabletMouse) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  EnterTablet();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  ExpectNoDragRecorded();
}

TEST_P(OverviewWindowDragHistogramTest, ToSnapSameDisplayTabletMouse) {
  EnterTablet();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(0, 300);
  ExpectNoDragRecorded();
}

using OverviewWindowDragHistogramTestMultiDisplayOnly =
    OverviewWindowDragHistogramTest;

TEST_P(OverviewWindowDragHistogramTestMultiDisplayOnly,
       ToGridOtherDisplayClamshellMouse) {
  UpdateDisplay("800x600,800x600");
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(GetSecondaryDisplay());
  generator->MoveMouseTo(1200, 300);
  generator->ReleaseLeftButton();
  ExpectDragRecorded(OverviewDragAction::kToGridOtherDisplayClamshellMouse);
}

TEST_P(OverviewWindowDragHistogramTestMultiDisplayOnly,
       ToDeskOtherDisplayClamshellMouse) {
  UpdateDisplay("800x600,800x600");
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(GetSecondaryDisplay());
  generator->MoveMouseTo(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/1u));
  generator->ReleaseLeftButton();
  ExpectDragRecorded(OverviewDragAction::kToDeskOtherDisplayClamshellMouse);
}

TEST_P(OverviewWindowDragHistogramTestMultiDisplayOnly,
       ToSnapOtherDisplayClamshellMouse) {
  UpdateDisplay("800x600,800x600");
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(GetSecondaryDisplay());
  generator->MoveMouseTo(800, 300);
  generator->ReleaseLeftButton();
  ExpectDragRecorded(OverviewDragAction::kToSnapOtherDisplayClamshellMouse);
}

INSTANTIATE_TEST_SUITE_P(All, OverviewWindowDragHistogramTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         OverviewWindowDragHistogramTestMultiDisplayOnly,
                         testing::Values(true));

}  // namespace ash
