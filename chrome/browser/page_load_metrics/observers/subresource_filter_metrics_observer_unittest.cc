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
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorSubresourceFilterMatch;
  SimulateTimingAndMetadataUpdate(timing, metadata);

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
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorSubresourceFilterMatch;
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
}
