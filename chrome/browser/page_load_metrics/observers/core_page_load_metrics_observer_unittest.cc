// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/rappor/rappor_utils.h"
#include "components/rappor/test_rappor_service.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

}  // namespace

class CorePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(make_scoped_ptr(new CorePageLoadMetricsObserver()));
  }

  void AssertNoHistogramsLogged() {
    histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded,
                                        0);
    histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
    histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
    histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    TestingBrowserProcess::GetGlobal()->SetRapporService(&rappor_tester_);
  }

  rappor::TestRapporService rappor_tester_;
};

TEST_F(CorePageLoadMetricsObserverTest, NoMetrics) {
  AssertNoHistogramsLogged();
}

TEST_F(CorePageLoadMetricsObserverTest, SamePageNoTriggerUntilTrueNavCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kDefaultTestUrlAnchor));
  // A same page navigation shouldn't trigger logging UMA for the original.
  AssertNoHistogramsLogged();

  // But we should keep the timing info and log it when we get another
  // navigation.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, SingleMetricAfterCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  AssertNoHistogramsLogged();

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, MultipleMetricsAfterCommits) {
  base::TimeDelta first_layout_1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta first_layout_2 = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta response = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta first_text_paint = base::TimeDelta::FromMilliseconds(30);
  base::TimeDelta dom_content = base::TimeDelta::FromMilliseconds(40);
  base::TimeDelta load = base::TimeDelta::FromMilliseconds(100);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout_1;
  timing.response_start = response;
  timing.first_text_paint = first_text_paint;
  timing.dom_content_loaded_event_start = dom_content;
  timing.load_event_start = load;

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kDefaultTestUrl2));

  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(200);
  timing2.first_layout = first_layout_2;

  SimulateTimingUpdate(timing2);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 2);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout_1.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 2);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout_1.InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout_2.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstTextPaint,
                                       first_text_paint.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramDomContentLoaded,
                                       dom_content.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramLoad,
                                       load.InMilliseconds(), 1);
}

TEST_F(CorePageLoadMetricsObserverTest, BackgroundDifferentHistogram) {
  base::TimeDelta first_layout = base::TimeDelta::FromSeconds(2);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  // Simulate "Open link in new tab."
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Simulate switching to the tab and making another navigation.
  web_contents()->WasShown();
  AssertNoHistogramsLogged();

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramFirstLayout,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstLayout, first_layout.InMilliseconds(),
      1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstTextPaint, 0);

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, OnlyBackgroundLaterEvents) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  // Set these events at 1 microsecond so they are definitely occur before we
  // background the tab later in the test.
  timing.response_start = base::TimeDelta::FromMicroseconds(1);
  timing.dom_content_loaded_event_start = base::TimeDelta::FromMicroseconds(1);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Background the tab, then forground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();
  timing.first_layout = base::TimeDelta::FromSeconds(3);
  timing.first_text_paint = base::TimeDelta::FromSeconds(4);
  SimulateTimingUpdate(timing);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramCommit, 0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramFirstLayout,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstLayout,
      timing.first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstTextPaint,
      timing.first_text_paint.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramDomContentLoaded,
      timing.dom_content_loaded_event_start.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, DontBackgroundQuickerLoad) {
  // Set this event at 1 microsecond so it occurs before we foreground later in
  // the test.
  base::TimeDelta first_layout = base::TimeDelta::FromMicroseconds(1);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  web_contents()->WasHidden();

  // Open in new tab
  StartNavigation(GURL(kDefaultTestUrl));

  // Switch to the tab
  web_contents()->WasShown();

  // Start another provisional load
  StartNavigation(GURL(kDefaultTestUrl2));
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationCommit(GURL(kDefaultTestUrl2));
  SimulateTimingUpdate(timing);
  rfh_tester->SimulateNavigationStop();

  // Navigate again to see if the timing updated for the foregrounded load.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedProvisionalLoad) {
  GURL url(kDefaultTestUrl);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(url);
  rfh_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);
  rfh_tester->SimulateNavigationStop();

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFailedProvisionalLoad,
                                      1);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedBackgroundProvisionalLoad) {
  // Test that failed provisional event does not get logged in the
  // histogram if it happened in the background
  GURL url(kDefaultTestUrl);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(url);
  web_contents()->WasHidden();
  rfh_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);
  rfh_tester->SimulateNavigationStop();

  histogram_tester().ExpectTotalCount(internal::kHistogramFailedProvisionalLoad,
                                      0);
}

TEST_F(CorePageLoadMetricsObserverTest, BackgroundBeforePaint) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_paint = base::TimeDelta::FromSeconds(10);
  NavigateAndCommit(GURL(kDefaultTestUrl));
  // Background the tab and go for a coffee or something.
  web_contents()->WasHidden();
  SimulateTimingUpdate(timing);
  // Come back and start browsing again.
  web_contents()->WasShown();
  // Simulate the user performaning another navigation.
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramBackgroundBeforePaint,
                                      1);
}

TEST_F(CorePageLoadMetricsObserverTest, NoRappor) {
  rappor::TestSample::Shadow* sample_obj =
      rappor_tester_.GetRecordedSampleForMetric(
          internal::kRapporMetricsNameCoarseTiming);
  EXPECT_EQ(sample_obj, nullptr);
}

TEST_F(CorePageLoadMetricsObserverTest, RapporLongPageLoad) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromSeconds(40);
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force logging RAPPOR.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  rappor::TestSample::Shadow* sample_obj =
      rappor_tester_.GetRecordedSampleForMetric(
          internal::kRapporMetricsNameCoarseTiming);
  const auto& string_it = sample_obj->string_fields.find("Domain");
  EXPECT_NE(string_it, sample_obj->string_fields.end());
  EXPECT_EQ(rappor::GetDomainAndRegistrySampleFromGURL(GURL(kDefaultTestUrl)),
            string_it->second);

  const auto& flag_it = sample_obj->flag_fields.find("IsSlow");
  EXPECT_NE(flag_it, sample_obj->flag_fields.end());
  EXPECT_EQ(1u, flag_it->second);
}

TEST_F(CorePageLoadMetricsObserverTest, RapporQuickPageLoad) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromSeconds(1);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force logging RAPPOR.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  rappor::TestSample::Shadow* sample_obj =
      rappor_tester_.GetRecordedSampleForMetric(
          internal::kRapporMetricsNameCoarseTiming);
  const auto& string_it = sample_obj->string_fields.find("Domain");
  EXPECT_NE(string_it, sample_obj->string_fields.end());
  EXPECT_EQ(rappor::GetDomainAndRegistrySampleFromGURL(GURL(kDefaultTestUrl)),
            string_it->second);

  const auto& flag_it = sample_obj->flag_fields.find("IsSlow");
  EXPECT_NE(flag_it, sample_obj->flag_fields.end());
  EXPECT_EQ(0u, flag_it->second);
}
