// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/metrics/histogram_samples.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#include "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The number of alive tabs at a renderer termination used by unit test.
const int kAliveTabsCountAtRendererTermination = 2;

// The number of timestamps added to the renderer termination timestamp list
// that are not counted in the RecentlyAliveTabs metric.
const int kExpiredTimesAddedCount = 2;

// URL constants used by TabUsageRecorderTest.
const char kURL[] = "http://www.chromium.org";
const char kNativeURL[] = "chrome://version";

}  // namespace

class TabUsageRecorderTest : public PlatformTest {
 protected:
  TabUsageRecorderTest()
      : web_state_list_(&web_state_list_delegate_),
        tab_usage_recorder_(&web_state_list_, nullptr) {}

  web::WebState* InsertTestWebState(const char* url, bool in_memory) {
    auto test_navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    test_navigation_manager->AddItem(GURL(url), ui::PAGE_TRANSITION_LINK);
    test_navigation_manager->SetLastCommittedItem(
        test_navigation_manager->GetItemAtIndex(
            test_navigation_manager->GetLastCommittedItemIndex()));

    auto test_web_state = std::make_unique<web::TestWebState>();
    test_web_state->SetNavigationManager(std::move(test_navigation_manager));
    test_web_state->SetIsEvicted(!in_memory);

    const int insertion_index = web_state_list_.InsertWebState(
        WebStateList::kInvalidIndex, std::move(test_web_state),
        WebStateList::INSERT_NO_FLAGS, WebStateOpener());

    return web_state_list_.GetWebStateAt(insertion_index);
  }

  void AddTimeToDequeInTabUsageRecorder(base::TimeTicks time) {
    tab_usage_recorder_.termination_timestamps_.push_back(time);
  }

  base::test::ScopedTaskEnvironment task_environment_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  base::HistogramTester histogram_tester_;
  TabUsageRecorder tab_usage_recorder_;
};

TEST_F(TabUsageRecorderTest, SwitchBetweenInMemoryTabs) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, true);

  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  histogram_tester_.ExpectUniqueSample(kSelectedTabHistogramName,
                                       TabUsageRecorder::IN_MEMORY, 1);
}

TEST_F(TabUsageRecorderTest, SwitchToEvictedTab) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);

  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  histogram_tester_.ExpectUniqueSample(kSelectedTabHistogramName,
                                       TabUsageRecorder::EVICTED, 1);
}

TEST_F(TabUsageRecorderTest, SwitchFromEvictedTab) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, true);

  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  histogram_tester_.ExpectUniqueSample(kSelectedTabHistogramName,
                                       TabUsageRecorder::IN_MEMORY, 1);
}

TEST_F(TabUsageRecorderTest, SwitchBetweenEvictedTabs) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);

  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  histogram_tester_.ExpectUniqueSample(kSelectedTabHistogramName,
                                       TabUsageRecorder::EVICTED, 1);
}

TEST_F(TabUsageRecorderTest, CountPageLoadsBeforeEvictedTab) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);

  // Call reload an arbitrary number of times.
  const int kNumReloads = 4;
  for (int i = 0; i < kNumReloads; i++) {
    tab_usage_recorder_.RecordPageLoadStart(mock_tab_a);
  }
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  histogram_tester_.ExpectUniqueSample(kPageLoadsBeforeEvictedTabSelected,
                                       kNumReloads, 1);
}

TEST_F(TabUsageRecorderTest, CountNativePageLoadsBeforeEvictedTab) {
  web::WebState* mock_tab_a = InsertTestWebState(kNativeURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kNativeURL, false);

  // Call reload an arbitrary number of times.
  const int kNumReloads = 4;
  for (int i = 0; i < kNumReloads; i++) {
    tab_usage_recorder_.RecordPageLoadStart(mock_tab_a);
  }
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  histogram_tester_.ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 0);
}

TEST_F(TabUsageRecorderTest, TestColdStartTabs) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_c = InsertTestWebState(kURL, false);
  // Set A and B as cold-start evicted tabs.  Leave C just evicted.
  std::vector<web::WebState*> cold_start_web_states = {
      mock_tab_a, mock_tab_b,
  };
  tab_usage_recorder_.InitialRestoredTabs(mock_tab_a, cold_start_web_states);

  // Switch from A (cold start evicted) to B (cold start evicted).
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  // Switch from B (cold start evicted) to C (evicted).
  tab_usage_recorder_.RecordTabSwitched(mock_tab_b, mock_tab_c);
  histogram_tester_.ExpectTotalCount(kSelectedTabHistogramName, 2);
  histogram_tester_.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::EVICTED_DUE_TO_COLD_START,
      1);
  histogram_tester_.ExpectBucketCount(kSelectedTabHistogramName,
                                      TabUsageRecorder::EVICTED, 1);
}

TEST_F(TabUsageRecorderTest, TestSwitchedModeTabs) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_c = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordPrimaryTabModelChange(false, nullptr);

  // Switch from A (incognito evicted) to B (incognito evicted).
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  // Switch from B (incognito evicted) to C (evicted).
  tab_usage_recorder_.RecordTabSwitched(mock_tab_b, mock_tab_c);
  histogram_tester_.ExpectTotalCount(kSelectedTabHistogramName, 2);
  histogram_tester_.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::EVICTED_DUE_TO_INCOGNITO, 0);
  histogram_tester_.ExpectBucketCount(kSelectedTabHistogramName,
                                      TabUsageRecorder::EVICTED, 2);
}

TEST_F(TabUsageRecorderTest, TestEvictedTabReloadTime) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  tab_usage_recorder_.RecordPageLoadDone(mock_tab_b, true);
  histogram_tester_.ExpectTotalCount(kEvictedTabReloadTime, 1);
}

TEST_F(TabUsageRecorderTest, TestEvictedTabReloadSuccess) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  tab_usage_recorder_.RecordPageLoadDone(mock_tab_b, true);
  histogram_tester_.ExpectUniqueSample(kEvictedTabReloadSuccessRate,
                                       TabUsageRecorder::LOAD_SUCCESS, 1);
}

TEST_F(TabUsageRecorderTest, TestEvictedTabReloadFailure) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  tab_usage_recorder_.RecordPageLoadDone(mock_tab_b, false);
  histogram_tester_.ExpectUniqueSample(kEvictedTabReloadSuccessRate,
                                       TabUsageRecorder::LOAD_FAILURE, 1);
}

TEST_F(TabUsageRecorderTest, TestUserWaitedForEvictedTabLoad) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  tab_usage_recorder_.RecordPageLoadDone(mock_tab_b, true);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_b, mock_tab_a);
  histogram_tester_.ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                       TabUsageRecorder::USER_WAITED, 1);
}

TEST_F(TabUsageRecorderTest, TestUserDidNotWaitForEvictedTabLoad) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_b, mock_tab_a);
  histogram_tester_.ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                       TabUsageRecorder::USER_DID_NOT_WAIT, 1);
}

TEST_F(TabUsageRecorderTest, TestUserBackgroundedDuringEvictedTabLoad) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  tab_usage_recorder_.AppDidEnterBackground();
  histogram_tester_.ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                       TabUsageRecorder::USER_LEFT_CHROME, 1);
}

TEST_F(TabUsageRecorderTest, TestTimeBetweenRestores) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  // Should record the time since launch until this page load begins.
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_b, mock_tab_a);
  // Should record the time since previous restore until this restore.
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_a);
  histogram_tester_.ExpectTotalCount(kTimeBetweenRestores, 2);
}

TEST_F(TabUsageRecorderTest, TestTimeAfterLastRestore) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, false);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);
  // Should record time since launch until background.
  tab_usage_recorder_.AppDidEnterBackground();
  tab_usage_recorder_.AppWillEnterForeground();
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);
  // Should record nothing.
  tab_usage_recorder_.RecordPageLoadStart(mock_tab_b);
  histogram_tester_.ExpectTotalCount(kTimeAfterLastRestore, 1);
}

// Verifies that metrics are recorded correctly when a renderer terminates.
TEST_F(TabUsageRecorderTest, RendererTerminated) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, false);

  // Add some extra WebStates that are not considered evicted so that
  // TabUsageRecorder count kAliveTabsCountAtRendererTermination tabs
  // as alive when mock_tab_a is evicted.
  for (int ii = 0; ii < kAliveTabsCountAtRendererTermination; ++ii) {
    ignore_result(InsertTestWebState(kURL, true));
  }

  base::TimeTicks now = base::TimeTicks::Now();

  // Add |kExpiredTimesAddedCount| expired timestamps and one recent timestamp
  // to the termination timestamp list.
  for (int seconds = kExpiredTimesAddedCount; seconds > 0; seconds--) {
    int expired_time_delta = kSecondsBeforeRendererTermination + seconds;
    AddTimeToDequeInTabUsageRecorder(
        now - base::TimeDelta::FromSeconds(expired_time_delta));
  }
  base::TimeTicks recent_time =
      now - base::TimeDelta::FromSeconds(kSecondsBeforeRendererTermination / 2);
  AddTimeToDequeInTabUsageRecorder(recent_time);

  tab_usage_recorder_.RendererTerminated(mock_tab_a, false, true);

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  BOOL saw_memory_warning =
      [defaults boolForKey:previous_session_info_constants::
                               kDidSeeMemoryWarningShortlyBeforeTerminating];
  histogram_tester_.ExpectUniqueSample(kRendererTerminationSawMemoryWarning,
                                       saw_memory_warning, 1);
  histogram_tester_.ExpectUniqueSample(kRendererTerminationAliveRenderers,
                                       kAliveTabsCountAtRendererTermination, 1);
  // Tests that the logged count of recently alive renderers is equal to the
  // live count at termination plus the recent termination and the
  // renderer terminated just now.
  histogram_tester_.ExpectUniqueSample(
      kRendererTerminationRecentlyAliveRenderers,
      kAliveTabsCountAtRendererTermination + 2, 1);
}

// Verifies that metrics are recorded correctly when a renderer terminated tab
// is switched to and reloaded.
TEST_F(TabUsageRecorderTest, SwitchToRendererTerminatedTab) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, false);

  tab_usage_recorder_.RendererTerminated(mock_tab_b, false, true);
  tab_usage_recorder_.RecordTabSwitched(mock_tab_a, mock_tab_b);

  histogram_tester_.ExpectUniqueSample(
      kSelectedTabHistogramName,
      TabUsageRecorder::EVICTED_DUE_TO_RENDERER_TERMINATION, 1);
}

// Verifies that Tab.StateAtRendererTermination metric is correctly reported
// when the application is in the foreground.
TEST_F(TabUsageRecorderTest, StateAtRendererTerminationForeground) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, true);

  tab_usage_recorder_.RendererTerminated(mock_tab_a, true, true);
  histogram_tester_.ExpectBucketCount(
      kRendererTerminationStateHistogram,
      TabUsageRecorder::FOREGROUND_TAB_FOREGROUND_APP, 1);

  tab_usage_recorder_.RendererTerminated(mock_tab_b, false, true);
  histogram_tester_.ExpectBucketCount(
      kRendererTerminationStateHistogram,
      TabUsageRecorder::BACKGROUND_TAB_FOREGROUND_APP, 1);
}

// Verifies that Tab.StateAtRendererTermination metric is correctly reported
// when the application is in the background.
TEST_F(TabUsageRecorderTest, StateAtRendererTerminationBackground) {
  web::WebState* mock_tab_a = InsertTestWebState(kURL, true);
  web::WebState* mock_tab_b = InsertTestWebState(kURL, true);

  tab_usage_recorder_.RendererTerminated(mock_tab_a, true, false);
  histogram_tester_.ExpectBucketCount(
      kRendererTerminationStateHistogram,
      TabUsageRecorder::FOREGROUND_TAB_BACKGROUND_APP, 1);

  tab_usage_recorder_.RendererTerminated(mock_tab_b, false, false);
  histogram_tester_.ExpectBucketCount(
      kRendererTerminationStateHistogram,
      TabUsageRecorder::BACKGROUND_TAB_BACKGROUND_APP, 1);
}
