// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/kill.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/metrics_navigation_throttle.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/page_load_metrics_messages.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

const char kDefaultTestUrl[] = "https://google.com/";
const char kDefaultTestUrlAnchor[] = "https://google.com/#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com/";
const char kFilteredStartUrl[] = "https://whatever.com/ignore-on-start";
const char kFilteredCommitUrl[] = "https://whatever.com/ignore-on-commit";

// Simple PageLoadMetricsObserver that copies observed PageLoadTimings into the
// provided std::vector, so they can be analyzed by unit tests.
class TestPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  TestPageLoadMetricsObserver(std::vector<PageLoadTiming>* updated_timings,
                              std::vector<PageLoadTiming>* complete_timings,
                              std::vector<GURL>* observed_committed_urls)
      : updated_timings_(updated_timings),
        complete_timings_(complete_timings),
        observed_committed_urls_(observed_committed_urls) {}

  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
               const GURL& currently_committed_url,
               bool started_in_foreground) override {
    observed_committed_urls_->push_back(currently_committed_url);
    return CONTINUE_OBSERVING;
  }

  void OnTimingUpdate(const PageLoadTiming& timing,
                      const PageLoadExtraInfo& extra_info) override {
    updated_timings_->push_back(timing);
  }

  void OnComplete(const PageLoadTiming& timing,
                  const PageLoadExtraInfo& extra_info) override {
    complete_timings_->push_back(timing);
  }

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info) override {
    return STOP_OBSERVING;
  }

 private:
  std::vector<PageLoadTiming>* const updated_timings_;
  std::vector<PageLoadTiming>* const complete_timings_;
  std::vector<GURL>* const observed_committed_urls_;
};

// Test PageLoadMetricsObserver that stops observing page loads with certain
// substrings in the URL.
class FilteringPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  explicit FilteringPageLoadMetricsObserver(
      std::vector<GURL>* completed_filtered_urls)
      : completed_filtered_urls_(completed_filtered_urls) {}

  ObservePolicy OnStart(content::NavigationHandle* handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-start") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  ObservePolicy OnCommit(content::NavigationHandle* handle) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-commit") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  void OnComplete(const PageLoadTiming& timing,
                  const PageLoadExtraInfo& extra_info) override {
    completed_filtered_urls_->push_back(extra_info.url);
  }

 private:
  std::vector<GURL>* const completed_filtered_urls_;
};

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface {
 public:
  TestPageLoadMetricsEmbedderInterface() : is_ntp_(false) {}

  bool IsNewTabPageUrl(const GURL& url) override { return is_ntp_; }
  void set_is_ntp(bool is_ntp) { is_ntp_ = is_ntp; }
  void RegisterObservers(PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<TestPageLoadMetricsObserver>(
        &updated_timings_, &complete_timings_, &observed_committed_urls_));
    tracker->AddObserver(base::MakeUnique<FilteringPageLoadMetricsObserver>(
        &completed_filtered_urls_));
  }
  const std::vector<PageLoadTiming>& updated_timings() const {
    return updated_timings_;
  }
  const std::vector<PageLoadTiming>& complete_timings() const {
    return complete_timings_;
  }

  // currently_committed_urls passed to OnStart().
  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return observed_committed_urls_;
  }

  // committed URLs passed to FilteringPageLoadMetricsObserver::OnComplete().
  const std::vector<GURL>& completed_filtered_urls() const {
    return completed_filtered_urls_;
  }

 private:
  std::vector<PageLoadTiming> updated_timings_;
  std::vector<PageLoadTiming> complete_timings_;
  std::vector<GURL> observed_committed_urls_;
  std::vector<GURL> completed_filtered_urls_;
  bool is_ntp_;
};

}  //  namespace

class MetricsWebContentsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() : num_errors_(0) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AttachObserver();
  }

  void SimulateTimingUpdate(const PageLoadTiming& timing) {
    SimulateTimingUpdate(timing, web_contents()->GetMainFrame());
  }

  void SimulateTimingUpdate(const PageLoadTiming& timing,
                            content::RenderFrameHost* render_frame_host) {
    ASSERT_TRUE(observer_->OnMessageReceived(
        PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing,
                                         PageLoadMetadata()),
        render_frame_host));
  }

  void AttachObserver() {
    embedder_interface_ = new TestPageLoadMetricsEmbedderInterface();
    // Owned by the web_contents. Tests must be careful not to call
    // SimulateTimingUpdate after they call DeleteContents() without also
    // calling AttachObserver() again. Otherwise they will use-after-free the
    // observer_.
    observer_ = MetricsWebContentsObserver::CreateForWebContents(
        web_contents(), base::WrapUnique(embedder_interface_));
    observer_->WasShown();
  }

  void CheckErrorEvent(InternalErrorLoadEvent error, int count) {
    histogram_tester_.ExpectBucketCount(internal::kErrorEvents, error, count);
    num_errors_ += count;
  }

  void CheckTotalErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, num_errors_);
  }

  void CheckNoErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, 0);
  }

  int CountEmptyCompleteTimingReported() {
    int empty = 0;
    for (const auto& timing : embedder_interface_->complete_timings()) {
      if (timing.IsEmpty())
        ++empty;
    }
    return empty;
  }

  int CountCompleteTimingReported() {
    return embedder_interface_->complete_timings().size();
  }
  int CountUpdatedTimingReported() {
    return embedder_interface_->updated_timings().size();
  }

  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return embedder_interface_->observed_committed_urls_from_on_start();
  }

  const std::vector<GURL>& completed_filtered_urls() const {
    return embedder_interface_->completed_filtered_urls();
  }

 protected:
  base::HistogramTester histogram_tester_;
  TestPageLoadMetricsEmbedderInterface* embedder_interface_;
  MetricsWebContentsObserver* observer_;

 private:
  int num_errors_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserverTest);
};

TEST_F(MetricsWebContentsObserverTest, SuccessfulMainFrameNavigation) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  ASSERT_TRUE(observed_committed_urls_from_on_start().empty());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(1u, observed_committed_urls_from_on_start().size());
  ASSERT_TRUE(observed_committed_urls_from_on_start().at(0).is_empty());

  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());
  ASSERT_EQ(2u, observed_committed_urls_from_on_start().size());
  ASSERT_EQ(kDefaultTestUrl,
            observed_committed_urls_from_on_start().at(1).spec());
  ASSERT_EQ(1, CountUpdatedTimingReported());

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, NotInMainFrame) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

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
  SimulateTimingUpdate(timing, subframe);
  subframe_tester->SimulateNavigationStop();

  // Navigate again to see if the timing updated for a subframe message.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(1, CountEmptyCompleteTimingReported());
  CheckErrorEvent(ERR_TIMING_IPC_FROM_SUBFRAME, 1);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, SamePageNoTrigger) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrlAnchor));
  // Send the same timing update. The original tracker for kDefaultTestUrl
  // should dedup the update, and the tracker for kDefaultTestUrlAnchor should
  // have been destroyed as a result of its being a same page navigation, so
  // CountUpdatedTimingReported() should continue to return 1.
  SimulateTimingUpdate(timing);

  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  // Navigate again to force histogram logging.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  // A same page navigation shouldn't trigger logging UMA for the original.
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(1, CountCompleteTimingReported());
  ASSERT_EQ(0, CountEmptyCompleteTimingReported());
  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogNewTabPage) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  embedder_interface_->set_is_ntp(true);

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogIrrelevantNavigation) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  GURL about_blank_url = GURL("about:blank");
  web_contents_tester->NavigateAndCommit(about_blank_url);
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  CheckErrorEvent(ERR_IPC_FROM_BAD_URL_SCHEME, 1);
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, NotInMainError) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

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
  SimulateTimingUpdate(timing, subframe);
  CheckErrorEvent(ERR_TIMING_IPC_FROM_SUBFRAME, 1);
  CheckTotalErrorEvents();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());
}

TEST_F(MetricsWebContentsObserverTest, BadIPC) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);
  PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(100);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing2);
  ASSERT_EQ(1, CountUpdatedTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, ObservePartialNavigation) {
  // Reset the state of the tests, and attach the MetricsWebContentsObserver in
  // the middle of a navigation. This tests that the class is robust to only
  // observing some of a navigation.
  DeleteContents();
  SetContents(CreateTestWebContents());

  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());

  // Start the navigation, then start observing the web contents. This used to
  // crash us. Make sure we bail out and don't log histograms.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  AttachObserver();
  rfh_tester->SimulateNavigationCommit(GURL(kDefaultTestUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force histogram logging.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(0, CountCompleteTimingReported());
  ASSERT_EQ(0, CountUpdatedTimingReported());
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogAbortChains) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  NavigateAndCommit(GURL(kDefaultTestUrl));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 0);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 2);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChains) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  // Start and abort three loads before one finally commits.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl2));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl2), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNewNavigation, 3,
                                      1);
  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChainsSameURL) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  // Start and abort three loads before one finally commits.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNewNavigation, 3,
                                      1);
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeSameURL, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeSameURL, 3, 1);
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChainsNoCommit) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  // Start and abort three loads before one finally commits.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl2));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl2), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents()->Stop();

  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNoCommit, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNoCommit, 3, 1);
}

TEST_F(MetricsWebContentsObserverTest, FlushMetricsOnAppEnterBackground) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester_.ExpectTotalCount(
      internal::kPageLoadCompletedAfterAppBackground, 0);

  observer_->FlushMetricsOnAppEnterBackground();

  histogram_tester_.ExpectTotalCount(
      internal::kPageLoadCompletedAfterAppBackground, 1);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, false, 1);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, true, 0);

  // Navigate again, which forces completion callbacks on the previous
  // navigation to be invoked.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));

  // Verify that, even though the page load completed, no complete timings were
  // reported, because the TestPageLoadMetricsObserver's
  // FlushMetricsOnAppEnterBackground implementation returned STOP_OBSERVING,
  // thus preventing OnComplete from being invoked.
  ASSERT_EQ(0, CountCompleteTimingReported());

  DeleteContents();

  histogram_tester_.ExpectTotalCount(
      internal::kPageLoadCompletedAfterAppBackground, 2);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, false, 1);
  histogram_tester_.ExpectBucketCount(
      internal::kPageLoadCompletedAfterAppBackground, true, 1);
}

TEST_F(MetricsWebContentsObserverTest, StopObservingOnCommit) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  ASSERT_TRUE(completed_filtered_urls().empty());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_TRUE(completed_filtered_urls().empty());

  // kFilteredCommitUrl should stop observing in OnCommit, and thus should not
  // reach OnComplete().
  web_contents_tester->NavigateAndCommit(GURL(kFilteredCommitUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl), GURL(kDefaultTestUrl2)}),
            completed_filtered_urls());
}

TEST_F(MetricsWebContentsObserverTest, StopObservingOnStart) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  ASSERT_TRUE(completed_filtered_urls().empty());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_TRUE(completed_filtered_urls().empty());

  // kFilteredCommitUrl should stop observing in OnStart, and thus should not
  // reach OnComplete().
  web_contents_tester->NavigateAndCommit(GURL(kFilteredStartUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl)}),
            completed_filtered_urls());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(std::vector<GURL>({GURL(kDefaultTestUrl), GURL(kDefaultTestUrl2)}),
            completed_filtered_urls());
}

}  // namespace page_load_metrics
