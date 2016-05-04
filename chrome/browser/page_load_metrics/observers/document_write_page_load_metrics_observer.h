// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramDocWriteParseStartToFirstContentfulPaint[];
extern const char kHistogramDocWriteBlockParseStartToFirstContentfulPaint[];
extern const char kHistogramDocWriteBlockReloadCount[];

}  // namespace internal

class DocumentWritePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DocumentWritePageLoadMetricsObserver();
  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnComplete(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  void LogDocumentWriteEvaluatorData(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  void LogDocumentWriteBlockData(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info);

  DISALLOW_COPY_AND_ASSIGN(DocumentWritePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DOCUMENT_WRITE_PAGE_LOAD_METRICS_OBSERVER_H_
