// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramServiceWorkerParseStart[];
extern const char kBackgroundHistogramServiceWorkerParseStart[];
extern const char kHistogramServiceWorkerFirstContentfulPaint[];
extern const char kBackgroundHistogramServiceWorkerFirstContentfulPaint[];
extern const char kHistogramServiceWorkerParseStartToFirstContentfulPaint[];
extern const char kHistogramServiceWorkerDomContentLoaded[];
extern const char kHistogramServiceWorkerLoad[];
extern const char kHistogramServiceWorkerFirstContentfulPaintInbox[];
extern const char
    kHistogramServiceWorkerParseStartToFirstContentfulPaintInbox[];
extern const char kHistogramServiceWorkerDomContentLoadedInbox[];
extern const char kHistogramServiceWorkerLoadInbox[];

}  // namespace internal

class ServiceWorkerPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ServiceWorkerPageLoadMetricsObserver();
  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnParseStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnLoadEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SERVICE_WORKER_PAGE_LOAD_METRICS_OBSERVER_H_
