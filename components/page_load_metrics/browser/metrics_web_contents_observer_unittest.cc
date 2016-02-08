// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

// Simple PageLoadMetricsObserver that copies observed PageLoadTimings into the
// provided std::vector, so they can be analyzed by unit tests.
class TestPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  explicit TestPageLoadMetricsObserver(
      std::vector<PageLoadTiming>* observed_timings)
      : observed_timings_(observed_timings) {}

  void OnComplete(const PageLoadTiming& timing,
                  const PageLoadExtraInfo&) override {
    observed_timings_->push_back(timing);
  }

 private:
  std::vector<PageLoadTiming>* const observed_timings_;
};

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface {
 public:
  TestPageLoadMetricsEmbedderInterface() : is_prerendering_(false) {}

  bool IsPrerendering(content::WebContents* web_contents) override {
    return is_prerendering_;
  }
  void set_is_prerendering(bool is_prerendering) {
    is_prerendering_ = is_prerendering;
  }
  void RegisterObservers(PageLoadTracker* tracker) override {
    tracker->AddObserver(
        make_scoped_ptr(new TestPageLoadMetricsObserver(&observed_timings_)));
  }
  const std::vector<PageLoadTiming>& observed_timings() const {
    return observed_timings_;
  }

 private:
  std::vector<PageLoadTiming> observed_timings_;
  bool is_prerendering_;
};

}  //  namespace

class MetricsWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() : num_errors_(0) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    AttachObserver();
  }

  void AttachObserver() {
    embedder_interface_ = new TestPageLoadMetricsEmbedderInterface();
    observer_.reset(new MetricsWebContentsObserver(
        web_contents(), make_scoped_ptr(embedder_interface_)));
    observer_->WasShown();
  }

  void CheckErrorEvent(InternalErrorLoadEvent error, int count) {
    histogram_tester_.ExpectBucketCount(internal::kErrorEvents, error, count);
    num_errors_ += count;
  }

  void CheckTotalEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, num_errors_);
  }

  void AssertNoNonEmptyTimingReported() {
    ASSERT_FALSE(embedder_interface_->observed_timings().empty());
    for (const auto& timing : embedder_interface_->observed_timings()) {
      ASSERT_TRUE(timing.IsEmpty());
    }
  }

  void AssertNoTimingReported() {
    ASSERT_TRUE(embedder_interface_->observed_timings().empty());
  }

 protected:
  base::HistogramTester histogram_tester_;
  TestPageLoadMetricsEmbedderInterface* embedder_interface_;
  scoped_ptr<MetricsWebContentsObserver> observer_;

 private:
  int num_errors_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserverTest);
};

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

  AssertNoNonEmptyTimingReported();
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
  AssertNoNonEmptyTimingReported();
}

TEST_F(MetricsWebContentsObserverTest, DontLogPrerender) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = base::TimeDelta::FromMilliseconds(10);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  embedder_interface_->set_is_prerendering(true);

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      web_contents()->GetMainFrame());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  AssertNoTimingReported();
}

TEST_F(MetricsWebContentsObserverTest, DontLogIrrelevantNavigation) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  GURL about_blank_url = GURL("about:blank");
  web_contents_tester->NavigateAndCommit(about_blank_url);

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      main_rfh());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  CheckErrorEvent(ERR_IPC_FROM_BAD_URL_SCHEME, 1);
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalEvents();
}

TEST_F(MetricsWebContentsObserverTest, NotInMainError) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = base::TimeDelta::FromMilliseconds(1);

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
  CheckErrorEvent(ERR_IPC_FROM_WRONG_FRAME, 1);
}

TEST_F(MetricsWebContentsObserverTest, BadIPC) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);
  PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(100);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      main_rfh());
  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing2),
      main_rfh());

  CheckErrorEvent(ERR_BAD_TIMING_IPC, 1);
  CheckTotalEvents();
}

TEST_F(MetricsWebContentsObserverTest, ObservePartialNavigation) {
  // Delete the observer for this test, add it once the navigation has started.
  observer_.reset();
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);
  timing.first_layout = base::TimeDelta::FromSeconds(2);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());

  // Start the navigation, then start observing the web contents. This used to
  // crash us. Make sure we bail out and don't log histograms.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  AttachObserver();
  rfh_tester->SimulateNavigationCommit(GURL(kDefaultTestUrl));

  observer_->OnMessageReceived(
      PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing),
      main_rfh());
  // Navigate again to force histogram logging.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  AssertNoTimingReported();
}

}  // namespace page_load_metrics
