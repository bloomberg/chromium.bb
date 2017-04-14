// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/delay_navigation_page_load_metrics_observer.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/experiments/delay_navigation_throttle.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDefaultTestUrl[] = "https://example.com/";

const int kDelayMillis = 100;

}  // namespace

// A WebContentsObserver that injects a DelayNavigationThrottle for main frame
// navigations on each call to DidStartNavigation.
class InjectDelayNavigationThrottleWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          InjectDelayNavigationThrottleWebContentsObserver> {
 public:
  static void CreateForWebContents(
      content::WebContents* web_contents,
      scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner);

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame())
      return;

    navigation_handle->RegisterThrottleForTesting(
        base::MakeUnique<DelayNavigationThrottle>(
            navigation_handle, mock_time_task_runner_,
            base::TimeDelta::FromMilliseconds(kDelayMillis)));
  }

 private:
  InjectDelayNavigationThrottleWebContentsObserver(
      content::WebContents* web_contents,
      scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner)
      : content::WebContentsObserver(web_contents),
        mock_time_task_runner_(mock_time_task_runner) {}

  friend class content::WebContentsUserData<
      InjectDelayNavigationThrottleWebContentsObserver>;

  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(InjectDelayNavigationThrottleWebContentsObserver);
};

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    InjectDelayNavigationThrottleWebContentsObserver);

void InjectDelayNavigationThrottleWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents,
    scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner) {
  InjectDelayNavigationThrottleWebContentsObserver* metrics =
      FromWebContents(web_contents);
  if (!metrics) {
    metrics = new InjectDelayNavigationThrottleWebContentsObserver(
        web_contents, mock_time_task_runner);
    web_contents->SetUserData(UserDataKey(), metrics);
  }
}

class DelayNavigationPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::MakeUnique<DelayNavigationPageLoadMetricsObserver>());
  }

  void SetUp() override {
    PageLoadMetricsObserverTestHarness::SetUp();
    mock_time_task_runner_ = new base::TestMockTimeTaskRunner();

    // Create a InjectDelayNavigationThrottleWebContentsObserver, which
    // instantiates a DelayNavigationThrottle for each main frame navigation in
    // the web_contents().
    InjectDelayNavigationThrottleWebContentsObserver::CreateForWebContents(
        web_contents(), mock_time_task_runner_);
  }

  bool AnyMetricsRecorded() {
    return !histogram_tester()
                .GetTotalCountsForPrefix("PageLoad.Clients.DelayNavigation.")
                .empty();
  }

  void NavigateToDefaultUrlAndCommit() {
    GURL url(kDefaultTestUrl);
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    rfh_tester->SimulateNavigationStart(url);

    // There may be other throttles that DEFER and post async tasks to the UI
    // thread. Allow them to run to completion, so our throttle is guaranteed to
    // have a chance to run.
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(mock_time_task_runner_->HasPendingTask());
    mock_time_task_runner_->FastForwardBy(
        base::TimeDelta::FromMilliseconds(kDelayMillis));
    EXPECT_FALSE(mock_time_task_runner_->HasPendingTask());

    // Run any remaining async tasks, to make sure all other deferred throttles
    // can complete.
    base::RunLoop().RunUntilIdle();

    rfh_tester->SimulateNavigationCommit(url);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;
};

TEST_F(DelayNavigationPageLoadMetricsObserverTest, NoMetricsWithoutNavigation) {
  ASSERT_FALSE(AnyMetricsRecorded());
}

TEST_F(DelayNavigationPageLoadMetricsObserverTest, CommitWithPaint) {
  NavigateToDefaultUrlAndCommit();

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing.first_paint = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);

  ASSERT_TRUE(AnyMetricsRecorded());
  histogram_tester().ExpectTotalCount(
      internal::kHistogramNavigationDelaySpecified, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramNavigationDelaySpecified, kDelayMillis, 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramNavigationDelayActual,
                                      1);
  histogram_tester().ExpectTotalCount(internal::kHistogramNavigationDelayDelta,
                                      1);
}
