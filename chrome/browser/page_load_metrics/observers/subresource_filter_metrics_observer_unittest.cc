// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_filter_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

namespace {
const char kDefaultTestUrl[] = "https://example.com/";
}  // namespace

class SubresourceFilterMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::MakeUnique<SubresourceFilterMetricsObserver>());
  }

  bool AnyMetricsRecorded() {
    return !histogram_tester()
                .GetTotalCountsForPrefix("PageLoad.Clients.SubresourceFilter.")
                .empty();
  }

  void InitializePageLoadTiming(page_load_metrics::PageLoadTiming* timing) {
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->parse_start = base::TimeDelta::FromMilliseconds(100);
    timing->parse_stop = base::TimeDelta::FromMilliseconds(200);
    timing->parse_blocked_on_script_load_duration =
        base::TimeDelta::FromMilliseconds(10);
    timing->parse_blocked_on_script_execution_duration =
        base::TimeDelta::FromMilliseconds(20);
    timing->first_contentful_paint = base::TimeDelta::FromMilliseconds(300);
    timing->first_meaningful_paint = base::TimeDelta::FromMilliseconds(400);
    timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(1200);
    timing->load_event_start = base::TimeDelta::FromMilliseconds(1500);
    PopulateRequiredTimingFields(timing);
  }
};

TEST_F(SubresourceFilterMetricsObserverTest,
       NoMetricsForNonSubresourceFilteredNavigation) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  page_load_metrics::PageLoadTiming timing;
  InitializePageLoadTiming(&timing);
  SimulateTimingUpdate(timing);

  // Navigate away from the current page to force logging of request and byte
  // metrics.
  NavigateToUntrackedUrl();

  ASSERT_FALSE(AnyMetricsRecorded());
}

TEST_F(SubresourceFilterMetricsObserverTest, Basic) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  page_load_metrics::PageLoadTiming timing;
  InitializePageLoadTiming(&timing);
  page_load_metrics::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  // Navigate away from the current page to force logging of metrics.
  NavigateToUntrackedUrl();

  ASSERT_TRUE(AnyMetricsRecorded());

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterCount, 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterFirstContentfulPaint,
      timing.first_contentful_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseStartToFirstContentfulPaint,
      (timing.first_contentful_paint.value() - timing.parse_start.value())
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterFirstMeaningfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterFirstMeaningfulPaint,
      timing.first_meaningful_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseStartToFirstMeaningfulPaint,
      (timing.first_meaningful_paint.value() - timing.parse_start.value())
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterDomContentLoaded,
      timing.dom_content_loaded_event_start.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramSubresourceFilterLoad,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterLoad,
      timing.load_event_start.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseDuration, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseDuration,
      (timing.parse_stop.value() - timing.parse_start.value()).InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptLoad, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptLoad,
      timing.parse_blocked_on_script_load_duration.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptExecution, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramSubresourceFilterParseBlockedOnScriptExecution,
      timing.parse_blocked_on_script_execution_duration.value()
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramSubresourceFilterForegroundDuration, 1);
}

TEST_F(SubresourceFilterMetricsObserverTest, Subresources) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  SimulateLoadedResource({false /* was_cached */,
                          1024 * 40 /* raw_body_bytes */,
                          false /* data_reduction_proxy_used */,
                          0 /* original_network_content_length */});

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  page_load_metrics::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  SimulateLoadedResource({false /* was_cached */,
                          1024 * 20 /* raw_body_bytes */,
                          false /* data_reduction_proxy_used */,
                          0 /* original_network_content_length */});

  SimulateLoadedResource({true /* was_cached */, 1024 * 10 /* raw_body_bytes */,
                          false /* data_reduction_proxy_used */,
                          0 /* original_network_content_length */});

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
  NavigateAndCommit(GURL(kDefaultTestUrl));

  SimulateMediaPlayed();

  SimulateLoadedResource({false /* was_cached */,
                          1024 * 40 /* raw_body_bytes */,
                          false /* data_reduction_proxy_used */,
                          0 /* original_network_content_length */});

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  page_load_metrics::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  SimulateLoadedResource({false /* was_cached */,
                          1024 * 20 /* raw_body_bytes */,
                          false /* data_reduction_proxy_used */,
                          0 /* original_network_content_length */});

  SimulateLoadedResource({true /* was_cached */, 1024 * 10 /* raw_body_bytes */,
                          false /* data_reduction_proxy_used */,
                          0 /* original_network_content_length */});

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
