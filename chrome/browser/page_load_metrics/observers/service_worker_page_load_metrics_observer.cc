// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace internal {

const char kHistogramServiceWorkerFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker.Timing2.NavigationToFirstContentfulPaint";
const char kBackgroundHistogramServiceWorkerFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker.Timing2.NavigationToFirstContentfulPaint."
    "Background";
const char kHistogramServiceWorkerParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker.Timing2.ParseStartToFirstContentfulPaint";
const char kHistogramServiceWorkerDomContentLoaded[] =
    "PageLoad.Clients.ServiceWorker.Timing2."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramServiceWorkerLoad[] =
    "PageLoad.Clients.ServiceWorker.Timing2.NavigationToLoadEventFired";

}  // namespace internal

ServiceWorkerPageLoadMetricsObserver::ServiceWorkerPageLoadMetricsObserver() {}

void ServiceWorkerPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.metadata.behavior_flags &
      blink::WebLoadingBehaviorFlag::
          WebLoadingBehaviorServiceWorkerControlled) {
    LogServiceWorkerHistograms(timing, info);
  }
}

void ServiceWorkerPageLoadMetricsObserver::LogServiceWorkerHistograms(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!timing.first_contentful_paint.is_zero()) {
    bool foreground_paint = WasStartedInForegroundEventInForeground(
        timing.first_contentful_paint, info);
    if (foreground_paint) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerFirstContentfulPaint,
                          timing.first_contentful_paint);
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint,
          timing.first_contentful_paint - timing.parse_start);
    } else {
      PAGE_LOAD_HISTOGRAM(
          internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint,
          timing.first_contentful_paint);
    }
  }

  if (WasStartedInForegroundEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerDomContentLoaded,
                        timing.dom_content_loaded_event_start);
  }
  if (WasStartedInForegroundEventInForeground(timing.load_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLoad,
                        timing.load_event_start);
  }
}
