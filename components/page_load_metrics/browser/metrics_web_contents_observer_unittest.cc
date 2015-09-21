// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

namespace page_load_metrics {

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

const char kHistogramNameFirstLayout[] =
    "PageLoad.Timing.NavigationToFirstLayout";
const char kHistogramNameDomContent[] =
    "PageLoad.Timing.NavigationToDOMContentLoadedEventFired";
const char kHistogramNameLoad[] =
    "PageLoad.Timing.NavigationToLoadEventFired";

}  //  namespace

class MetricsWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    observer_ = make_scoped_ptr(new MetricsWebContentsObserver(web_contents()));
  }

  void AssertNoHistogramsLogged() {
    histogram_tester_.ExpectTotalCount(kHistogramNameDomContent, 0);
    histogram_tester_.ExpectTotalCount(kHistogramNameLoad, 0);
    histogram_tester_.ExpectTotalCount(kHistogramNameFirstLayout, 0);
  }

 protected:
  base::HistogramTester histogram_tester_;
  scoped_ptr<MetricsWebContentsObserver> observer_;
};

TEST_F(MetricsWebContentsObserverTest, NoMetrics) {
  AssertNoHistogramsLogged();
}

TEST_F(MetricsWebContentsObserverTest, NotInMainFrame) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationStart(GURL(kDefaultTestUrl2));
  subframe_tester->SimulateNavigationCommit(GURL(kDefaultTestUrl2));
  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      subframe);
  subframe_tester->SimulateNavigationStop();

  // Navigate again to see if the timing updated for a subframe message.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  AssertNoHistogramsLogged();
}

TEST_F(MetricsWebContentsObserverTest, SamePageNoTrigger) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrlAnchor));
  // A same page navigation shouldn't trigger logging UMA for the original.
  AssertNoHistogramsLogged();
}

TEST_F(MetricsWebContentsObserverTest, SamePageNoTriggerUntilTrueNavCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrlAnchor));
  // A same page navigation shouldn't trigger logging UMA for the original.
  AssertNoHistogramsLogged();

  // But we should keep the timing info and log it when we get another
  // navigation.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester_.ExpectTotalCount(kHistogramNameDomContent, 0);
  histogram_tester_.ExpectTotalCount(kHistogramNameLoad, 0);
  histogram_tester_.ExpectTotalCount(kHistogramNameFirstLayout, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFirstLayout,
                                      first_layout.InMilliseconds(), 1);
}

TEST_F(MetricsWebContentsObserverTest, SingleMetricAfterCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  AssertNoHistogramsLogged();

  // Navigate again to force histogram recording.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester_.ExpectTotalCount(kHistogramNameDomContent, 0);
  histogram_tester_.ExpectTotalCount(kHistogramNameLoad, 0);
  histogram_tester_.ExpectTotalCount(kHistogramNameFirstLayout, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFirstLayout,
                                      first_layout.InMilliseconds(), 1);
}

TEST_F(MetricsWebContentsObserverTest, MultipleMetricsAfterCommits) {
  base::TimeDelta first_layout_1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta first_layout_2 = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta response = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta dom_content = base::TimeDelta::FromMilliseconds(40);
  base::TimeDelta load = base::TimeDelta::FromMilliseconds(100);

  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout_1;
  timing.response_start = response;
  timing.dom_content_loaded_event_start = dom_content;
  timing.load_event_start = load;

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(200);
  timing2.first_layout = first_layout_2;

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing2),
      web_contents()->GetMainFrame());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester_.ExpectBucketCount(kHistogramNameFirstLayout,
                                      first_layout_1.InMilliseconds(), 1);
  histogram_tester_.ExpectTotalCount(kHistogramNameFirstLayout, 2);
  histogram_tester_.ExpectBucketCount(kHistogramNameFirstLayout,
                                      first_layout_1.InMilliseconds(), 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameFirstLayout,
                                      first_layout_2.InMilliseconds(), 1);

  histogram_tester_.ExpectTotalCount(kHistogramNameDomContent, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameDomContent,
                                      dom_content.InMilliseconds(), 1);

  histogram_tester_.ExpectTotalCount(kHistogramNameLoad, 1);
  histogram_tester_.ExpectBucketCount(kHistogramNameLoad, load.InMilliseconds(),
                                      1);
}

}  // namespace page_load_metrics
