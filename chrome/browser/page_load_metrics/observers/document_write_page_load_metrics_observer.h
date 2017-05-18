// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramDocWriteParseStartToFirstContentfulPaint[];
extern const char kHistogramDocWriteBlockParseStartToFirstContentfulPaint[];
extern const char kHistogramDocWriteBlockCount[];
extern const char kHistogramDocWriteBlockReloadCount[];

}  // namespace internal

class DocumentWritePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DocumentWritePageLoadMetricsObserver() = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

  void OnParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

  void OnLoadingBehaviorObserved(
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

  enum DocumentWriteLoadingBehavior {
    LOADING_BEHAVIOR_BLOCK,
    LOADING_BEHAVIOR_RELOAD,
    LOADING_BEHAVIOR_SAME_SITE_DIFF_SCHEME,
    LOADING_BEHAVIOR_MAX
  };

 private:
  void LogDocumentWriteEvaluatorFirstContentfulPaint(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  void LogDocumentWriteEvaluatorFirstMeaningfulPaint(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  void LogDocumentWriteEvaluatorParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  void LogDocumentWriteBlockFirstContentfulPaint(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  void LogDocumentWriteBlockParseStop(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  void LogDocumentWriteBlockFirstMeaningfulPaint(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  bool doc_write_same_site_diff_scheme_ = false;
  bool doc_write_block_observed_ = false;
  bool doc_write_block_reload_observed_ = false;

  DISALLOW_COPY_AND_ASSIGN(DocumentWritePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_
