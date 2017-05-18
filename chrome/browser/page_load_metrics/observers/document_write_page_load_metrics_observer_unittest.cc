// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/document_write_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

class DocumentWritePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::MakeUnique<DocumentWritePageLoadMetricsObserver>());
  }
  void AssertNoPreloadHistogramsLogged() {
    histogram_tester().ExpectTotalCount(
        internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 0);
  }

  void AssertNoBlockHistogramsLogged() {
    histogram_tester().ExpectTotalCount(
        internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 0);
  }
};

TEST_F(DocumentWritePageLoadMetricsObserverTest, NoMetrics) {
  AssertNoPreloadHistogramsLogged();
  AssertNoBlockHistogramsLogged();
}

TEST_F(DocumentWritePageLoadMetricsObserverTest, PossiblePreload) {
  base::TimeDelta contentful_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint = contentful_paint;
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteEvaluator;
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingAndMetadataUpdate(timing, metadata);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint,
      contentful_paint.InMilliseconds(), 1);

  NavigateAndCommit(GURL("https://www.example.com"));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramDocWriteParseStartToFirstContentfulPaint,
      contentful_paint.InMilliseconds(), 1);
}

TEST_F(DocumentWritePageLoadMetricsObserverTest, NoPossiblePreload) {
  base::TimeDelta contentful_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint = contentful_paint;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadMetadata metadata;
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingAndMetadataUpdate(timing, metadata);
  NavigateAndCommit(GURL("https://www.example.com"));
  AssertNoPreloadHistogramsLogged();
}

TEST_F(DocumentWritePageLoadMetricsObserverTest, PossibleBlock) {
  base::TimeDelta contentful_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint = contentful_paint;
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlock;
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingAndMetadataUpdate(timing, metadata);

  histogram_tester().ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
      contentful_paint.InMilliseconds(), 1);

  NavigateAndCommit(GURL("https://www.example.com"));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramDocWriteBlockParseStartToFirstContentfulPaint,
      contentful_paint.InMilliseconds(), 1);
}

TEST_F(DocumentWritePageLoadMetricsObserverTest, PossibleBlockReload) {
  base::TimeDelta contentful_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint = contentful_paint;
  timing.parse_timing->parse_start = base::TimeDelta::FromMilliseconds(1);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadMetadata metadata;
  metadata.behavior_flags |= blink::WebLoadingBehaviorFlag::
      kWebLoadingBehaviorDocumentWriteBlockReload;
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingAndMetadataUpdate(timing, metadata);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 1);

  // Another reload.
  NavigateAndCommit(GURL("https://www.example.com"));
  SimulateTimingAndMetadataUpdate(timing, metadata);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 2);

  // Another metadata update should not increase reload count.
  metadata.behavior_flags |=
      blink::WebLoadingBehaviorFlag::kWebLoadingBehaviorServiceWorkerControlled;
  SimulateTimingAndMetadataUpdate(timing, metadata);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramDocWriteBlockReloadCount, 2);

  histogram_tester().ExpectTotalCount(internal::kHistogramDocWriteBlockCount,
                                      0);
}

TEST_F(DocumentWritePageLoadMetricsObserverTest, NoPossibleBlock) {
  base::TimeDelta contentful_paint = base::TimeDelta::FromMilliseconds(1);
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.paint_timing->first_contentful_paint = contentful_paint;
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::PageLoadMetadata metadata;
  NavigateAndCommit(GURL("https://www.google.com"));
  SimulateTimingAndMetadataUpdate(timing, metadata);

  NavigateAndCommit(GURL("https://www.example.com"));
  AssertNoBlockHistogramsLogged();
}
