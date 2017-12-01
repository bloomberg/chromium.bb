// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_filter_metrics_observer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "url/gurl.h"

namespace {
const char kDefaultTestUrl[] = "https://example.com/";
const char kDefaultTestUrlWithActivation[] = "https://example-activation.com/";
const char kDefaultTestUrlWithActivationDryRun[] =
    "https://dryrun.example-activation.com/";
}  // namespace

class SubresourceFilterMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  SubresourceFilterMetricsObserverTest() {}
  ~SubresourceFilterMetricsObserverTest() override {}

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    subresource_filter::SubresourceFilterObserverManager::CreateForWebContents(
        web_contents());
    observer_manager_ =
        subresource_filter::SubresourceFilterObserverManager::FromWebContents(
            web_contents());
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<SubresourceFilterMetricsObserver>());
  }

  size_t TotalMetricsRecorded() {
    return histogram_tester()
        .GetTotalCountsForPrefix("PageLoad.Clients.SubresourceFilter.")
        .size();
  }

  void InitializePageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->parse_timing->parse_start = base::TimeDelta::FromMilliseconds(100);
    timing->parse_timing->parse_stop = base::TimeDelta::FromMilliseconds(200);
    timing->parse_timing->parse_blocked_on_script_load_duration =
        base::TimeDelta::FromMilliseconds(10);
    timing->parse_timing->parse_blocked_on_script_execution_duration =
        base::TimeDelta::FromMilliseconds(20);
    timing->paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing->paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(400);
    timing->document_timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(1200);
    timing->document_timing->load_event_start =
        base::TimeDelta::FromMilliseconds(1500);
    PopulateRequiredTimingFields(timing);
  }

  void SimulateNavigateAndCommit(const GURL& url) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->Start();
    // Simulate an activation notification.
    content::NavigationHandle* handle = simulator->GetNavigationHandle();
    if (handle->GetURL() == kDefaultTestUrlWithActivation) {
      observer_manager_->NotifyPageActivationComputed(
          handle, subresource_filter::ActivationDecision::ACTIVATED,
          subresource_filter::ActivationState(
              subresource_filter::ActivationLevel::ENABLED));
    } else if (handle->GetURL() == kDefaultTestUrlWithActivationDryRun) {
      observer_manager_->NotifyPageActivationComputed(
          handle, subresource_filter::ActivationDecision::ACTIVATED,
          subresource_filter::ActivationState(
              subresource_filter::ActivationLevel::DRYRUN));
    } else {
      observer_manager_->NotifyPageActivationComputed(
          handle,
          subresource_filter::ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
          subresource_filter::ActivationState(
              subresource_filter::ActivationLevel::DISABLED));
    }
    simulator->Commit();
  }

  void ExpectActivationDecision(const char* url,
                                subresource_filter::ActivationDecision decision,
                                subresource_filter::ActivationLevel level) {
    histogram_tester().ExpectBucketCount(
        internal::kHistogramSubresourceFilterActivationDecision,
        static_cast<int>(decision), 1);

    const auto& entries = test_ukm_recorder().GetEntriesByName(
        internal::kUkmSubresourceFilterName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* entry : entries) {
      test_ukm_recorder().ExpectEntrySourceHasUrl(entry, GURL(url));
      test_ukm_recorder().ExpectEntryMetric(
          entry, internal::kUkmSubresourceFilterActivationDecision,
          static_cast<int64_t>(decision));
      if (level == subresource_filter::ActivationLevel::DRYRUN) {
        test_ukm_recorder().ExpectEntryMetric(
            entry, internal::kUkmSubresourceFilterDryRun, true);
      }
    }
  }

 private:
  // Owned by the WebContents.
  subresource_filter::SubresourceFilterObserverManager* observer_manager_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterMetricsObserverTest);
};

TEST_F(SubresourceFilterMetricsObserverTest,
       NoMetricsForNonSubresourceFilteredNavigation) {
  SimulateNavigateAndCommit(GURL(kDefaultTestUrl));

  page_load_metrics::mojom::PageLoadTiming timing;
  InitializePageLoadTiming(&timing);
  SimulateTimingUpdate(timing);

  // Navigate away from the current page to force logging of request and byte
  // metrics.
  NavigateToUntrackedUrl();

  EXPECT_EQ(1u, TotalMetricsRecorded());
  ExpectActivationDecision(
      kDefaultTestUrl,
      subresource_filter::ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
      subresource_filter::ActivationLevel::DISABLED);
}

TEST_F(SubresourceFilterMetricsObserverTest, Basic) {
  SimulateNavigateAndCommit(GURL(kDefaultTestUrlWithActivation));

  page_load_metrics::mojom::PageLoadTiming timing;
  InitializePageLoadTiming(&timing);
  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  // Navigate away from the current page to force logging of metrics.
  NavigateToUntrackedUrl();

  EXPECT_GT(TotalMetricsRecorded(), 0u);
  ExpectActivationDecision(kDefaultTestUrlWithActivation,
                           subresource_filter::ActivationDecision::ACTIVATED,
                           subresource_filter::ActivationLevel::ENABLED);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCount, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseStartToFirstContentfulPaint,
      (timing.paint_timing->first_contentful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterFirstMeaningfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterFirstMeaningfulPaint,
      timing.paint_timing->first_meaningful_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint,
      (timing.paint_timing->first_meaningful_paint.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value()
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(internal::kHistogramSubresourceFilterLoad,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterLoad,
      timing.document_timing->load_event_start.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseDuration, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseDuration,
      (timing.parse_timing->parse_stop.value() -
       timing.parse_timing->parse_start.value())
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptLoad, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptLoad,
      timing.parse_timing->parse_blocked_on_script_load_duration.value()
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptExecution, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptExecution,
      timing.parse_timing->parse_blocked_on_script_execution_duration.value()
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterForegroundDuration, 1);
}

TEST_F(SubresourceFilterMetricsObserverTest, DryRun) {
  SimulateNavigateAndCommit(GURL(kDefaultTestUrlWithActivationDryRun));

  page_load_metrics::mojom::PageLoadTiming timing;
  InitializePageLoadTiming(&timing);
  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  // Navigate away from the current page to force logging of metrics.
  NavigateToUntrackedUrl();

  EXPECT_GT(TotalMetricsRecorded(), 0u);
  ExpectActivationDecision(kDefaultTestUrlWithActivationDryRun,
                           subresource_filter::ActivationDecision::ACTIVATED,
                           subresource_filter::ActivationLevel::DRYRUN);
}

TEST_F(SubresourceFilterMetricsObserverTest, Subresources) {
  SimulateNavigateAndCommit(GURL(kDefaultTestUrlWithActivation));

  SimulateLoadedResource({GURL(kResourceUrl), net::HostPortPair(),
                          -1 /* frame_tree_node_id */, false /* was_cached */,
                          1024 * 40 /* raw_body_bytes */,
                          0 /* original_network_content_length */,
                          nullptr /* data_reduction_proxy_data */,
                          content::ResourceType::RESOURCE_TYPE_SCRIPT, 0,
                          nullptr /* load_timing_info */});

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  SimulateLoadedResource({GURL(kResourceUrl), net::HostPortPair(),
                          -1 /* frame_tree_node_id */, false /* was_cached */,
                          1024 * 20 /* raw_body_bytes */,
                          0 /* original_network_content_length */,
                          nullptr /* data_reduction_proxy_data */,
                          content::ResourceType::RESOURCE_TYPE_SCRIPT, 0,
                          nullptr /* load_timing_info */});

  SimulateLoadedResource({GURL(kResourceUrl), net::HostPortPair(),
                          -1 /* frame_tree_node_id */, true /* was_cached */,
                          1024 * 10 /* raw_body_bytes */,
                          0 /* original_network_content_length */,
                          nullptr /* data_reduction_proxy_data */,
                          content::ResourceType::RESOURCE_TYPE_SCRIPT, 0,
                          nullptr /* load_timing_info */});

  ExpectActivationDecision(kDefaultTestUrlWithActivation,
                           subresource_filter::ActivationDecision::ACTIVATED,
                           subresource_filter::ActivationLevel::ENABLED);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCount, 1);

  // Navigate away from the current page to force logging of request and byte
  // metrics.
  NavigateToUntrackedUrl();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterTotalResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterTotalResources, 3, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNetworkResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNetworkResources, 2, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCacheResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterCacheResources, 1, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterTotalBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterTotalBytes, 70, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNetworkBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNetworkBytes, 60, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCacheBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterCacheBytes, 10, 1);

  // Make sure data is also recorded for pages with no media.
  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNoMediaTotalResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNoMediaTotalResources, 3, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNoMediaNetworkResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNoMediaNetworkResources, 2, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNoMediaCacheResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNoMediaCacheResources, 1, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNoMediaTotalBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNoMediaTotalBytes, 70, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNoMediaNetworkBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNoMediaNetworkBytes, 60, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNoMediaCacheBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNoMediaCacheBytes, 10, 1);
}

TEST_F(SubresourceFilterMetricsObserverTest, SubresourcesWithMedia) {
  SimulateNavigateAndCommit(GURL(kDefaultTestUrlWithActivation));

  SimulateMediaPlayed();

  SimulateLoadedResource({GURL(kResourceUrl), net::HostPortPair(),
                          -1 /* frame_tree_node_id */, false /* was_cached */,
                          1024 * 40 /* raw_body_bytes */,
                          0 /* original_network_content_length */,
                          nullptr /* data_reduction_proxy_data */,
                          content::ResourceType::RESOURCE_TYPE_SCRIPT, 0,
                          nullptr /* load_timing_info */});

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  SimulateLoadedResource({GURL(kResourceUrl), net::HostPortPair(),
                          -1 /* frame_tree_node_id */, false /* was_cached */,
                          1024 * 20 /* raw_body_bytes */,
                          0 /* original_network_content_length */,
                          nullptr /* data_reduction_proxy_data */,
                          content::ResourceType::RESOURCE_TYPE_SCRIPT, 0,
                          nullptr /* load_timing_info */});

  SimulateLoadedResource({GURL(kResourceUrl), net::HostPortPair(),
                          -1 /* frame_tree_node_id */, true /* was_cached */,
                          1024 * 10 /* raw_body_bytes */,
                          0 /* original_network_content_length */,
                          nullptr /* data_reduction_proxy_data */,
                          content::ResourceType::RESOURCE_TYPE_SCRIPT, 0,
                          nullptr /* load_timing_info */});

  ExpectActivationDecision(kDefaultTestUrlWithActivation,
                           subresource_filter::ActivationDecision::ACTIVATED,
                           subresource_filter::ActivationLevel::ENABLED);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCount, 1);

  // Navigate away from the current page to force logging of request and byte
  // metrics.
  NavigateToUntrackedUrl();

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterTotalResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterTotalResources, 3, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNetworkResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNetworkResources, 2, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCacheResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterCacheResources, 1, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterTotalBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterTotalBytes, 70, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterNetworkBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterNetworkBytes, 60, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCacheBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterCacheBytes, 10, 1);

  // Make sure data is also recorded for pages with media.
  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterMediaTotalResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterMediaTotalResources, 3, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterMediaNetworkResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterMediaNetworkResources, 2, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterMediaCacheResources, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterMediaCacheResources, 1, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterMediaTotalBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterMediaTotalBytes, 70, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterMediaNetworkBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterMediaNetworkBytes, 60, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterMediaCacheBytes, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterMediaCacheBytes, 10, 1);
}
