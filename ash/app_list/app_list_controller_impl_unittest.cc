// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_popup_view.h"

namespace ash {

namespace {

using ::app_list::kAppListResultLaunchIndexAndQueryLength;
using ::app_list::kAppListTileLaunchIndexAndQueryLength;
using ::app_list::SearchResultLaunchLocation;

bool IsTabletMode() {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

app_list::AppListView* GetAppListView() {
  return Shell::Get()->app_list_controller()->presenter()->GetView();
}

bool GetExpandArrowViewVisibility() {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->expand_arrow_view()
      ->GetVisible();
}

app_list::SearchBoxView* GetSearchBoxView() {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->GetSearchBoxView();
}

aura::Window* GetVirtualKeyboardWindow() {
  return Shell::Get()
      ->ash_keyboard_controller()
      ->keyboard_controller()
      ->GetKeyboardWindow();
}

void ShowAppListNow() {
  Shell::Get()->app_list_controller()->presenter()->Show(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      base::TimeTicks::Now());
}

void DismissAppListNow() {
  Shell::Get()->app_list_controller()->presenter()->Dismiss(
      base::TimeTicks::Now());
}

aura::Window* GetAppListViewNativeWindow() {
  return GetAppListView()->get_fullscreen_widget_for_test()->GetNativeView();
}

void SetSearchText(AppListControllerImpl* controller, const std::string& text) {
  controller->GetSearchModel()->search_box()->Update(base::ASCIIToUTF16(text),
                                                     false);
}

}  // namespace

class AppListControllerImplTest : public AshTestBase {
 public:
  AppListControllerImplTest() = default;
  ~AppListControllerImplTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplTest);
};

// Hide the expand arrow view in tablet mode when there is no activatable window
// (see https://crbug.com/923089).
TEST_F(AppListControllerImplTest, UpdateExpandArrowViewVisibility) {
  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_TRUE(IsTabletMode());

  // No activatable windows. So hide the expand arrow view.
  EXPECT_FALSE(GetExpandArrowViewVisibility());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());

  // Activate w1 then press home launcher button. Expand arrow view should show
  // because w1 still exists.
  wm::ActivateWindow(w1.get());
  Shell::Get()
      ->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->ShowHomeLauncher(display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(WindowStateType::kMinimized,
            wm::GetWindowState(w1.get())->GetStateType());
  EXPECT_TRUE(GetExpandArrowViewVisibility());

  // Activate w2 then close w1. w2 still exists so expand arrow view shows.
  wm::ActivateWindow(w2.get());
  w1.reset();
  EXPECT_TRUE(GetExpandArrowViewVisibility());

  // No activatable windows. Hide the expand arrow view.
  w2.reset();
  EXPECT_FALSE(GetExpandArrowViewVisibility());
}

// In clamshell mode, when the AppListView's bottom is on the display edge
// and app list state is HALF, the rounded corners should be hidden
// (https://crbug.com/942084).
TEST_F(AppListControllerImplTest, HideRoundingCorners) {
  Shell::Get()->ash_keyboard_controller()->SetEnableFlag(
      keyboard::mojom::KeyboardEnableFlag::kShelfEnabled);

  // Show the app list view and click on the search box with mouse. So the
  // VirtualKeyboard is shown.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);

  // Wait until the virtual keyboard shows on the screen.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Test the following things:
  // (1) AppListView is at the top of the screen.
  // (2) AppListView's state is HALF.
  // (3) AppListBackgroundShield is translated to hide the rounded corners.
  aura::Window* native_window =
      GetAppListView()->get_fullscreen_widget_for_test()->GetNativeView();
  gfx::Rect app_list_screen_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(0, app_list_screen_bounds.y());
  EXPECT_EQ(ash::AppListViewState::kHalf, GetAppListView()->app_list_state());
  gfx::Transform expected_transform;
  expected_transform.Translate(0, -app_list::kAppListBackgroundRadius);
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());

  // Set the search box inactive and wait until the virtual keyboard is hidden.
  GetSearchBoxView()->SetSearchBoxActive(false, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Test that the rounded corners should show again.
  expected_transform = gfx::Transform();
  EXPECT_EQ(
      expected_transform,
      GetAppListView()->GetAppListBackgroundShieldForTest()->GetTransform());
}

// Verifies that in clamshell mode the bounds of AppListView are correct when
// the AppListView is in PEEKING state and the virtual keyboard is enabled (see
// https://crbug.com/944233).
TEST_F(AppListControllerImplTest, CheckAppListViewBoundsWhenVKeyboardEnabled) {
  Shell::Get()->ash_keyboard_controller()->SetEnableFlag(
      keyboard::mojom::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView and click on the search box with mouse. So the
  // VirtualKeyboard is shown. Wait until the virtual keyboard shows.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Hide the AppListView. Wait until the virtual keyboard is hidden as well.
  DismissAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Show the AppListView again. Check the following things:
  // (1) Virtual keyboard does not show.
  // (2) AppListView is in PEEKING state.
  // (3) AppListView's bounds are the same as the preferred bounds for
  // the PEEKING state.
  ShowAppListNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::AppListViewState::kPeeking,
            GetAppListView()->app_list_state());
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());
  EXPECT_EQ(GetAppListView()->GetPreferredWidgetBoundsForState(
                ash::AppListViewState::kPeeking),
            GetAppListViewNativeWindow()->bounds());
}

// Verifies that in tablet mode, the AppListView has correct bounds when the
// virtual keyboard is dismissed (see https://crbug.com/944133).
TEST_F(AppListControllerImplTest, CheckAppListViewBoundsWhenDismissVKeyboard) {
  Shell::Get()->ash_keyboard_controller()->SetEnableFlag(
      keyboard::mojom::KeyboardEnableFlag::kShelfEnabled);

  // Show the AppListView and click on the search box with mouse so the
  // VirtualKeyboard is shown. Wait until the virtual keyboard shows.
  ShowAppListNow();
  GetSearchBoxView()->SetSearchBoxActive(true, ui::ET_MOUSE_PRESSED);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Turn on the tablet mode. The virtual keyboard should still show.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_TRUE(IsTabletMode());
  EXPECT_TRUE(GetVirtualKeyboardWindow()->IsVisible());

  // Close the virtual keyboard. Wait until it is hidden.
  Shell::Get()->ash_keyboard_controller()->HideKeyboard(
      mojom::HideReason::kUser);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, GetVirtualKeyboardWindow());

  // Check the following things:
  // (1) AppListView's state is FULLSCREEN_SEARCH
  // (2) AppListView's bounds are the same as the preferred bounds for
  // the FULLSCREEN_SEARCH state.
  EXPECT_EQ(ash::AppListViewState::kFullscreenSearch,
            GetAppListView()->app_list_state());
  EXPECT_EQ(GetAppListView()->GetPreferredWidgetBoundsForState(
                ash::AppListViewState::kFullscreenSearch),
            GetAppListViewNativeWindow()->bounds());
}

// Verifies that closing notification by gesture should not dismiss the AppList.
// (see https://crbug.com/948344)
TEST_F(AppListControllerImplTest, CloseNotificationWithAppListShown) {
  ShowAppListNow();

  // Add one notification.
  ASSERT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
  const std::string notification_id("id");
  const std::string notification_title("title");
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_BASE_FORMAT, notification_id,
          base::UTF8ToUTF16(notification_title),
          base::UTF8ToUTF16("test message"), gfx::Image(),
          base::string16() /* display_source */, GURL(),
          message_center::NotifierId(), message_center::RichNotificationData(),
          new message_center::NotificationDelegate()));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(
      1u, message_center::MessageCenter::Get()->GetPopupNotifications().size());

  // Calculate the drag start point and end point.
  UnifiedSystemTrayTestApi test_api(GetPrimaryUnifiedSystemTray());
  message_center::MessagePopupView* popup_view =
      test_api.GetPopupViewForNotificationID(notification_id);
  ASSERT_TRUE(popup_view);
  gfx::Rect bounds_in_screen = popup_view->GetBoundsInScreen();
  const gfx::Point drag_start = bounds_in_screen.left_center();
  const gfx::Point drag_end = bounds_in_screen.right_center();

  // Swipe away notification by gesture. Verifies that AppListView still shows.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->GestureScrollSequence(
      drag_start, drag_end, base::TimeDelta::FromMicroseconds(500), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetAppListView());
  EXPECT_EQ(
      0u, message_center::MessageCenter::Get()->GetPopupNotifications().size());
}

class AppListControllerImplMetricsTest : public AshTestBase {
 public:
  AppListControllerImplMetricsTest() = default;
  ~AppListControllerImplMetricsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = ash::Shell::Get()->app_list_controller();
    ash::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }

  void TearDown() override {
    ash::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    AshTestBase::TearDown();
  }

  AppListControllerImpl* controller_;
  const base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplMetricsTest);
};

TEST_F(AppListControllerImplMetricsTest, LogSingleResultListClick) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  SetSearchText(controller_, "");
  controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kResultList,
                                        4);
  histogram_tester_.ExpectUniqueSample(kAppListResultLaunchIndexAndQueryLength,
                                       4, 1);
}

TEST_F(AppListControllerImplMetricsTest, LogSingleTileListClick) {
  histogram_tester_.ExpectTotalCount(kAppListTileLaunchIndexAndQueryLength, 0);
  SetSearchText(controller_, "aaaa");
  controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kTileList,
                                        4);
  histogram_tester_.ExpectUniqueSample(kAppListTileLaunchIndexAndQueryLength,
                                       32, 1);
}

TEST_F(AppListControllerImplMetricsTest, LogOneClickInEveryBucket) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  for (int query_length = 0; query_length < 11; ++query_length) {
    const std::string query(query_length, 'a');
    for (int click_index = 0; click_index < 7; ++click_index) {
      SetSearchText(controller_, query);
      controller_->LogResultLaunchHistogram(
          SearchResultLaunchLocation::kResultList, click_index);
    }
  }

  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     77);
  for (int query_length = 0; query_length < 11; ++query_length) {
    for (int click_index = 0; click_index < 7; ++click_index) {
      histogram_tester_.ExpectBucketCount(
          kAppListResultLaunchIndexAndQueryLength,
          7 * query_length + click_index, 1);
    }
  }
}

TEST_F(AppListControllerImplMetricsTest, LogManyClicksInOneBucket) {
  histogram_tester_.ExpectTotalCount(kAppListTileLaunchIndexAndQueryLength, 0);
  SetSearchText(controller_, "aaaa");
  for (int i = 0; i < 50; ++i)
    controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kTileList,
                                          4);
  histogram_tester_.ExpectUniqueSample(kAppListTileLaunchIndexAndQueryLength,
                                       32, 50);
}

// Verifies that the PresentationTimeRecorder works correctly for the home
// launcher gesture drag in tablet mode (https://crbug.com/947105).
TEST_F(AppListControllerImplMetricsTest,
       PresentationTimeRecordedForDragInTabletMode) {
  // Wait until the construction of TabletModeController finishes.
  base::RunLoop().RunUntilIdle();

  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_TRUE(IsTabletMode());

  // Create a window then press the home launcher button. Expect that |w| is
  // hidden.
  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  Shell::Get()
      ->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->ShowHomeLauncher(display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_FALSE(w->IsVisible());
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  int delta_y = 1;
  gfx::Point start = GetAppListView()
                         ->get_fullscreen_widget_for_test()
                         ->GetWindowBoundsInScreen()
                         .top_right();
  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Emulate to drag the launcher downward.
  // Send SCROLL_START event. Check the presentation metrics values.
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, delta_y));
  GetAppListView()->OnGestureEvent(&start_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Send SCROLL_UPDATE event. Check the presentation metrics values.
  timestamp += base::TimeDelta::FromMilliseconds(25);
  delta_y += 20;
  start.Offset(0, 1);
  ui::GestureEvent update_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
  GetAppListView()->OnGestureEvent(&update_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 1);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Send SCROLL_END event. Check the presentation metrics values.
  timestamp += base::TimeDelta::FromMilliseconds(25);
  start.Offset(0, 1);
  ui::GestureEvent end_event =
      ui::GestureEvent(start.x(), start.y() + delta_y, ui::EF_NONE, timestamp,
                       ui::GestureEventDetails(ui::ET_GESTURE_END));
  GetAppListView()->OnGestureEvent(&end_event);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 1);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 1);

  // After the gesture scroll event ends, the window shows.
  EXPECT_TRUE(w->IsVisible());
  ASSERT_TRUE(IsTabletMode());
}

// One edge case may do harm to the presentation metrics reporter for tablet
// mode: the user may keep pressing on launcher while exiting the tablet mode by
// rotating the lid. In this situation, OnHomeLauncherDragEnd is not triggered.
// It is handled correctly now because the AppListView is always closed after
// exiting the tablet mode. But it still has potential risk to break in future.
// Write this test case for precaution (https://crbug.com/947105).
TEST_F(AppListControllerImplMetricsTest,
       PresentationMetricsForTabletNotRecordedInClamshell) {
  // Wait until the construction of TabletModeController finishes.
  base::RunLoop().RunUntilIdle();

  // Turn on the tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_TRUE(IsTabletMode());

  // Create a window then press the home launcher button. Expect that |w| is
  // hidden.
  std::unique_ptr<aura::Window> w(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  Shell::Get()
      ->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->ShowHomeLauncher(display::Screen::GetScreen()->GetPrimaryDisplay());
  EXPECT_FALSE(w->IsVisible());
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());

  gfx::Point start = GetAppListView()
                         ->get_fullscreen_widget_for_test()
                         ->GetWindowBoundsInScreen()
                         .top_right();
  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Emulate to drag the launcher downward.
  // Send SCROLL_START event. Check the presentation metrics values.
  ui::GestureEvent start_event = ui::GestureEvent(
      start.x(), start.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 1));
  GetAppListView()->OnGestureEvent(&start_event);

  // Turn off the tablet mode before scrolling is finished.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(IsTabletMode());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetAppListView());

  // Check metrics initial values.
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // Emulate to drag launcher from shelf. Then verifies the following things:
  // (1) Metrics values for tablet mode are not recorded.
  // (2) Metrics values for clamshell mode are recorded correctly.
  gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();
  shelf_bounds.Intersect(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  gfx::Point shelf_center = shelf_bounds.CenterPoint();
  gfx::Point target_point =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(shelf_center, target_point,
                                   base::TimeDelta::FromMicroseconds(500), 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AppListViewState::kFullscreenAllApps,
            GetAppListView()->app_list_state());
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.TabletMode", 0);
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.TabletMode", 0);

  // AppListView::UpdateYPositionAndOpacity is triggered by
  // ShelfLayoutManager::StartGestureDrag and
  // ShelfLayoutManager::UpdateGestureDrag. Note that scrolling step of event
  // generator is 1. So the expected value is 2.
  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.ClamshellMode", 2);

  histogram_tester_.ExpectTotalCount(
      "Apps.StateTransition.Drag.PresentationTime.MaxLatency.ClamshellMode", 1);
}

}  // namespace ash
