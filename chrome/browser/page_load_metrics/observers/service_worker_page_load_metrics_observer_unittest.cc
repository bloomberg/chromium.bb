// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kInboxTestUrl[] = "https://inbox.google.com/test";

}  // namespace

class ServiceWorkerPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::MakeUnique<ServiceWorkerPageLoadMetricsObserver>());
  }

  void SimulateTimingWithoutPaint() {
    page_load_metrics::PageLoadTiming timing;
    timing.navigation_start = base::Time::FromDoubleT(1);
    SimulateTimingUpdate(timing);
  }

  void AssertNoServiceWorkerHistogramsLogged() {
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerFirstContentfulPaint, 0);
    histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 0);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 0);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerDomContentLoaded, 0);
    histogram_tester().ExpectTotalCount(internal::kHistogramServiceWorkerLoad,
                                        0);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerParseStart, 0);
    histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramServiceWorkerParseStart, 0);
  }

  void AssertNoInboxHistogramsLogged() {
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerFirstContentfulPaintInbox, 0);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintInbox,
        0);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerDomContentLoadedInbox, 0);
    histogram_tester().ExpectTotalCount(
        internal::kHistogramServiceWorkerLoadInbox, 0);
  }

  void InitializeTestPageLoadTiming(page_load_metrics::PageLoadTiming* timing) {
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->parse_start = base::TimeDelta::FromMilliseconds(100);
    timing->first_contentful_paint = base::TimeDelta::FromMilliseconds(300);
    timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(600);
    timing->load_event_start = base::TimeDelta::FromMilliseconds(1000);
    PopulateRequiredTimingFields(timing);
  }
};

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, NoMetrics) {
  AssertNoServiceWorkerHistogramsLogged();
  AssertNoInboxHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, NoServiceWorker) {
  page_load_metrics::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  AssertNoServiceWorkerHistogramsLogged();
  AssertNoInboxHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, WithServiceWorker) {
  page_load_metrics::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  page_load_metrics::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorServiceWorkerControlled;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.first_contentful_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 0);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint,
      (timing.first_contentful_paint.value() - timing.parse_start.value())
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerDomContentLoaded,
      timing.dom_content_loaded_event_start.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramServiceWorkerLoad, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerLoad,
      timing.load_event_start.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStart, 1);

  AssertNoInboxHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, WithServiceWorkerBackground) {
  page_load_metrics::PageLoadTiming timing;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorServiceWorkerControlled;

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingAndMetadataUpdate(timing, metadata);

  // Background the tab, then forground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();

  InitializeTestPageLoadTiming(&timing);
  SimulateTimingAndMetadataUpdate(timing, metadata);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint,
      timing.first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramServiceWorkerLoad, 0);
  // TODO(crbug.com/686590): The following expectation fails on Win7 Tests
  // (dbg)(1) builder, so is disabled for the time being.
  // histogram_tester().ExpectTotalCount(
  //     internal::kBackgroundHistogramServiceWorkerParseStart, 1);

  AssertNoInboxHistogramsLogged();
}

TEST_F(ServiceWorkerPageLoadMetricsObserverTest, InboxSite) {
  page_load_metrics::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kInboxTestUrl));
  page_load_metrics::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::WebLoadingBehaviorServiceWorkerControlled;
  SimulateTimingAndMetadataUpdate(timing, metadata);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaint,
      timing.first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintInbox, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerFirstContentfulPaintInbox,
      timing.first_contentful_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint, 0);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint,
      (timing.first_contentful_paint.value() - timing.parse_start.value())
          .InMilliseconds(),
      1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintInbox,
      1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintInbox,
      (timing.first_contentful_paint.value() - timing.parse_start.value())
          .InMilliseconds(),
      1);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerDomContentLoaded,
      timing.dom_content_loaded_event_start.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerDomContentLoadedInbox, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerDomContentLoadedInbox,
      timing.dom_content_loaded_event_start.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramServiceWorkerLoad, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerLoad,
      timing.load_event_start.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerLoadInbox, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramServiceWorkerLoadInbox,
      timing.load_event_start.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramServiceWorkerParseStart, 1);
}
