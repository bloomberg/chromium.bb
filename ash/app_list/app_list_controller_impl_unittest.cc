// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/strings/string16.cc"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"

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

bool GetExpandArrowViewVisibility() {
  return Shell::Get()
      ->app_list_controller()
      ->presenter()
      ->GetView()
      ->app_list_main_view()
      ->contents_view()
      ->expand_arrow_view()
      ->visible();
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
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED,
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

class AppListControllerImplMetricsTest : public AshTestBase {
 public:
  AppListControllerImplMetricsTest() = default;
  ~AppListControllerImplMetricsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = ash::Shell::Get()->app_list_controller();
  }

  AppListControllerImpl* controller_;
  const base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerImplMetricsTest);
};

TEST_F(AppListControllerImplMetricsTest, LogSingleResultListClick) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  controller_->StartSearch(base::string16());
  controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kResultList,
                                        4);
  histogram_tester_.ExpectUniqueSample(kAppListResultLaunchIndexAndQueryLength,
                                       4, 1);
}

TEST_F(AppListControllerImplMetricsTest, LogSingleTileListClick) {
  histogram_tester_.ExpectTotalCount(kAppListTileLaunchIndexAndQueryLength, 0);
  controller_->StartSearch(base::ASCIIToUTF16("aaaa"));
  controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kTileList,
                                        4);
  histogram_tester_.ExpectUniqueSample(kAppListTileLaunchIndexAndQueryLength,
                                       32, 1);
}

TEST_F(AppListControllerImplMetricsTest, LogOneClickInEveryBucket) {
  histogram_tester_.ExpectTotalCount(kAppListResultLaunchIndexAndQueryLength,
                                     0);
  for (int query_length = 0; query_length < 11; ++query_length) {
    const base::string16 query =
        base::ASCIIToUTF16(std::string(query_length, 'a'));
    for (int click_index = 0; click_index < 7; ++click_index) {
      controller_->StartSearch(query);
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
  controller_->StartSearch(base::ASCIIToUTF16("aaaa"));
  for (int i = 0; i < 50; ++i)
    controller_->LogResultLaunchHistogram(SearchResultLaunchLocation::kTileList,
                                          4);
  histogram_tester_.ExpectUniqueSample(kAppListTileLaunchIndexAndQueryLength,
                                       32, 50);
}

}  // namespace ash
