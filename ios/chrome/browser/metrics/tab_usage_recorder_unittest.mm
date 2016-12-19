// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_samples.h"
#include "base/stl_util.h"
#include "base/test/histogram_tester.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#include "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_delegate.h"
#import "ios/chrome/browser/tabs/tab.h"
#include "ios/testing/ocmock_complex_type_helper.h"
#include "ios/web/public/test/test_web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/ocmock_extensions.h"

@interface TURTestTabMock : OCMockComplexTypeHelper {
  GURL _url;
}

@property(nonatomic, assign) const GURL& url;
@end

@implementation TURTestTabMock
- (const GURL&)url {
  return _url;
}
- (void)setUrl:(const GURL&)url {
  _url = url;
}
@end

// A mock TabUsageRecorderDelegate which allows the unit tests to control
// the count of live tabs returned from the |liveTabsCount| delegate method.
@interface MockTabUsageRecorderDelegate : NSObject<TabUsageRecorderDelegate> {
  NSUInteger _tabCount;
}

// Sets the live tab count returned from the |liveTabsCount| delegate method.
- (void)setLiveTabsCount:(NSUInteger)count;

@end

@implementation MockTabUsageRecorderDelegate

- (void)setLiveTabsCount:(NSUInteger)count {
  _tabCount = count;
}

- (NSUInteger)liveTabsCount {
  return _tabCount;
}

@end

namespace {

// The number of alive tabs at a renderer termination used by unit test.
const NSUInteger kAliveTabsCountAtRendererTermination = 2U;

// The number of timestamps added to the renderer termination timestamp list
// that are not counted in the RecentlyAliveTabs metric.
const int kExpiredTimesAddedCount = 2;

class TabUsageRecorderForTesting : public TabUsageRecorder {
 public:
  TabUsageRecorderForTesting(MockTabUsageRecorderDelegate* delegate)
      : TabUsageRecorder(delegate) {}
  // For testing only.
  base::TimeTicks RestoreStartTime() const { return restore_start_time_; }

  // Adds |time| to the deque keeping track of renderer termination
  // timestamps.
  void AddTimeToDeque(base::TimeTicks time) {
    termination_timestamps_.push_back(time);
  }
};

class TabUsageRecorderTest : public PlatformTest {
 protected:
  void SetUp() override {
    loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
    ui_thread_.reset(new web::TestWebThread(web::WebThread::UI, loop_.get()));
    histogram_tester_.reset(new base::HistogramTester());
    // Set the delegate to nil to allow the relevant unit tests direct access to
    // the mock delegate.
    tab_usage_recorder_.reset(new TabUsageRecorderForTesting(nil));
    webUrl_ = GURL("http://www.chromium.org");
    nativeUrl_ = GURL("chrome://version");
  }

  id MockTab(bool inMemory) {
    id tab_mock = [[TURTestTabMock alloc]
        initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];
    id web_controller_mock =
        [OCMockObject mockForClass:[CRWWebController class]];
    [[[tab_mock stub] andReturn:web_controller_mock] webController];
    [[[tab_mock stub] andReturnBool:false] isPrerenderTab];
    [tab_mock setUrl:webUrl_];
    [[[web_controller_mock stub] andReturnBool:inMemory] isViewAlive];
    [[web_controller_mock stub] removeObserver:OCMOCK_ANY];
    return [tab_mock autorelease];
  }

  GURL webUrl_;
  GURL nativeUrl_;
  std::unique_ptr<base::MessageLoop> loop_;
  std::unique_ptr<web::TestWebThread> ui_thread_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<TabUsageRecorderForTesting> tab_usage_recorder_;
};

TEST_F(TabUsageRecorderTest, SwitchBetweenInMemoryTabs) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(true);

  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  histogram_tester_->ExpectUniqueSample(kSelectedTabHistogramName,
                                        TabUsageRecorder::IN_MEMORY, 1);
}

TEST_F(TabUsageRecorderTest, SwitchToEvictedTab) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);

  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  histogram_tester_->ExpectUniqueSample(kSelectedTabHistogramName,
                                        TabUsageRecorder::EVICTED, 1);
}

TEST_F(TabUsageRecorderTest, SwitchFromEvictedTab) {
  id tab_mock_a = MockTab(false);
  id tab_mock_b = MockTab(true);

  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  histogram_tester_->ExpectUniqueSample(kSelectedTabHistogramName,
                                        TabUsageRecorder::IN_MEMORY, 1);
}

TEST_F(TabUsageRecorderTest, SwitchBetweenEvictedTabs) {
  id tab_mock_a = MockTab(false);
  id tab_mock_b = MockTab(false);

  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  histogram_tester_->ExpectUniqueSample(kSelectedTabHistogramName,
                                        TabUsageRecorder::EVICTED, 1);
}

TEST_F(TabUsageRecorderTest, CountPageLoadsBeforeEvictedTab) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);

  // Call reload an arbitrary number of times.
  const int kNumReloads = 4;
  for (int i = 0; i < kNumReloads; i++) {
    tab_usage_recorder_->RecordPageLoadStart(tab_mock_a);
  }
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  histogram_tester_->ExpectUniqueSample(kPageLoadsBeforeEvictedTabSelected,
                                        kNumReloads, 1);
}

TEST_F(TabUsageRecorderTest, CountNativePageLoadsBeforeEvictedTab) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);
  [tab_mock_a setUrl:nativeUrl_];
  [tab_mock_b setUrl:nativeUrl_];

  // Call reload an arbitrary number of times.
  const int kNumReloads = 4;
  for (int i = 0; i < kNumReloads; i++) {
    tab_usage_recorder_->RecordPageLoadStart(tab_mock_a);
  }
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  histogram_tester_->ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 0);
}

TEST_F(TabUsageRecorderTest, TestColdStartTabs) {
  id tab_mock_a = MockTab(false);
  id tab_mock_b = MockTab(false);
  id tab_mock_c = MockTab(false);
  // Set A and B as cold-start evicted tabs.  Leave C just evicted.
  NSMutableArray* cold_start_tabs = [NSMutableArray array];
  [cold_start_tabs addObject:tab_mock_a];
  [cold_start_tabs addObject:tab_mock_b];
  tab_usage_recorder_->InitialRestoredTabs(tab_mock_a, cold_start_tabs);

  // Switch from A (cold start evicted) to B (cold start evicted).
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  // Switch from B (cold start evicted) to C (evicted).
  tab_usage_recorder_->RecordTabSwitched(tab_mock_b, tab_mock_c);
  histogram_tester_->ExpectTotalCount(kSelectedTabHistogramName, 2);
  histogram_tester_->ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::EVICTED_DUE_TO_COLD_START,
      1);
  histogram_tester_->ExpectBucketCount(kSelectedTabHistogramName,
                                       TabUsageRecorder::EVICTED, 1);
}

TEST_F(TabUsageRecorderTest, TestSwitchedModeTabs) {
  id tab_mock_a = MockTab(false);
  id tab_mock_b = MockTab(false);
  id tab_mock_c = MockTab(false);
  NSMutableArray* switch_to_incognito_tabs = [NSMutableArray array];
  [switch_to_incognito_tabs addObject:tab_mock_a];
  [switch_to_incognito_tabs addObject:tab_mock_b];
  tab_usage_recorder_->RecordPrimaryTabModelChange(false, nil);

  // Switch from A (incognito evicted) to B (incognito evicted).
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  // Switch from B (incognito evicted) to C (evicted).
  tab_usage_recorder_->RecordTabSwitched(tab_mock_b, tab_mock_c);
  histogram_tester_->ExpectTotalCount(kSelectedTabHistogramName, 2);
  histogram_tester_->ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::EVICTED_DUE_TO_INCOGNITO, 0);
  histogram_tester_->ExpectBucketCount(kSelectedTabHistogramName,
                                       TabUsageRecorder::EVICTED, 2);
}

TEST_F(TabUsageRecorderTest, TestEvictedTabReloadTime) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  tab_usage_recorder_->RecordPageLoadDone(tab_mock_b, true);
  histogram_tester_->ExpectTotalCount(kEvictedTabReloadTime, 1);
}

TEST_F(TabUsageRecorderTest, TestEvictedTabReloadSuccess) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  tab_usage_recorder_->RecordPageLoadDone(tab_mock_b, true);
  histogram_tester_->ExpectUniqueSample(kEvictedTabReloadSuccessRate,
                                        TabUsageRecorder::LOAD_SUCCESS, 1);
}

TEST_F(TabUsageRecorderTest, TestEvictedTabReloadFailure) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  tab_usage_recorder_->RecordPageLoadDone(tab_mock_b, false);
  histogram_tester_->ExpectUniqueSample(kEvictedTabReloadSuccessRate,
                                        TabUsageRecorder::LOAD_FAILURE, 1);
}

TEST_F(TabUsageRecorderTest, TestUserWaitedForEvictedTabLoad) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  tab_usage_recorder_->RecordPageLoadDone(tab_mock_b, true);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_b, tab_mock_a);
  histogram_tester_->ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                        TabUsageRecorder::USER_WAITED, 1);
}

TEST_F(TabUsageRecorderTest, TestUserDidNotWaitForEvictedTabLoad) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_b, tab_mock_a);
  histogram_tester_->ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                        TabUsageRecorder::USER_DID_NOT_WAIT, 1);
}

TEST_F(TabUsageRecorderTest, TestUserBackgroundedDuringEvictedTabLoad) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  tab_usage_recorder_->AppDidEnterBackground();
  histogram_tester_->ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                        TabUsageRecorder::USER_LEFT_CHROME, 1);
}

TEST_F(TabUsageRecorderTest, TestTimeBetweenRestores) {
  id tab_mock_a = MockTab(false);
  id tab_mock_b = MockTab(false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  // Should record the time since launch until this page load begins.
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_b, tab_mock_a);
  // Should record the time since previous restore until this restore.
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_a);
  histogram_tester_->ExpectTotalCount(kTimeBetweenRestores, 2);
}

TEST_F(TabUsageRecorderTest, TestTimeAfterLastRestore) {
  id tab_mock_a = MockTab(false);
  id tab_mock_b = MockTab(false);
  // Should record time since launch until background.
  tab_usage_recorder_->AppDidEnterBackground();
  tab_usage_recorder_->AppWillEnterForeground();
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);
  // Should record nothing.
  tab_usage_recorder_->RecordPageLoadStart(tab_mock_b);
  histogram_tester_->ExpectTotalCount(kTimeAfterLastRestore, 1);
}

// Verifies that metrics are recorded correctly when a renderer terminates.
TEST_F(TabUsageRecorderTest, RendererTerminated) {
  Tab* terminated_tab = MockTab(false);

  // Set up the delegate to return |kAliveTabsCountAtRenderTermination|.
  base::scoped_nsobject<MockTabUsageRecorderDelegate> delegate(
      [[MockTabUsageRecorderDelegate alloc] init]);
  [delegate setLiveTabsCount:kAliveTabsCountAtRendererTermination];
  tab_usage_recorder_->SetDelegate(delegate);

  base::TimeTicks now = base::TimeTicks::Now();

  // Add |kExpiredTimesAddedCount| expired timestamps and one recent timestamp
  // to the termination timestamp list.
  for (int seconds = kExpiredTimesAddedCount; seconds > 0; seconds--) {
    int expired_time_delta = kSecondsBeforeRendererTermination + seconds;
    tab_usage_recorder_->AddTimeToDeque(
        now - base::TimeDelta::FromSeconds(expired_time_delta));
  }
  base::TimeTicks recent_time =
      now - base::TimeDelta::FromSeconds(kSecondsBeforeRendererTermination / 2);
  tab_usage_recorder_->AddTimeToDeque(recent_time);

  tab_usage_recorder_->RendererTerminated(terminated_tab, false);

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  BOOL saw_memory_warning =
      [defaults boolForKey:previous_session_info_constants::
                               kDidSeeMemoryWarningShortlyBeforeTerminating];
  histogram_tester_->ExpectUniqueSample(kRendererTerminationSawMemoryWarning,
                                        saw_memory_warning, 1);
  histogram_tester_->ExpectUniqueSample(kRendererTerminationAliveRenderers,
                                        kAliveTabsCountAtRendererTermination,
                                        1);
  // Tests that the logged count of recently alive renderers is equal to the
  // live count at termination plus the recent termination and the
  // renderer terminated just now.
  histogram_tester_->ExpectUniqueSample(
      kRendererTerminationRecentlyAliveRenderers,
      kAliveTabsCountAtRendererTermination + 2, 1);
}

// Verifies that metrics are recorded correctly when a renderer terminated tab
// is switched to and reloaded.
TEST_F(TabUsageRecorderTest, SwitchToRendererTerminatedTab) {
  id tab_mock_a = MockTab(true);
  id tab_mock_b = MockTab(false);

  tab_usage_recorder_->RendererTerminated(tab_mock_b, false);
  tab_usage_recorder_->RecordTabSwitched(tab_mock_a, tab_mock_b);

  histogram_tester_->ExpectUniqueSample(
      kSelectedTabHistogramName,
      TabUsageRecorder::EVICTED_DUE_TO_RENDERER_TERMINATION, 1);
}

}  // namespace
