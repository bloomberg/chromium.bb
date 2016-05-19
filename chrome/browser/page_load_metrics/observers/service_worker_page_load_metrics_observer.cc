// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace internal {

const char kHistogramServiceWorkerFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kBackgroundHistogramServiceWorkerFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker.PaintTiming."
    "NavigationToFirstContentfulPaint.Background";
const char kHistogramServiceWorkerParseStartToFirstContentfulPaint[] =
    "PageLoad.Clients.ServiceWorker.PaintTiming."
    "ParseStartToFirstContentfulPaint";
const char kHistogramServiceWorkerDomContentLoaded[] =
    "PageLoad.Clients.ServiceWorker.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramServiceWorkerLoad[] =
    "PageLoad.Clients.ServiceWorker.DocumentTiming.NavigationToLoadEventFired";

}  // namespace internal

namespace {

bool IsServiceWorkerControlled(
    const page_load_metrics::PageLoadExtraInfo& info) {
  return (info.metadata.behavior_flags &
          blink::WebLoadingBehaviorFlag::
              WebLoadingBehaviorServiceWorkerControlled) != 0;
}

}  // namespace

ServiceWorkerPageLoadMetricsObserver::ServiceWorkerPageLoadMetricsObserver() {}

void ServiceWorkerPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!IsServiceWorkerControlled(info))
    return;
  if (WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                              info)) {
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

void ServiceWorkerPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!IsServiceWorkerControlled(info))
    return;
  if (WasStartedInForegroundEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerDomContentLoaded,
                        timing.dom_content_loaded_event_start);
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!IsServiceWorkerControlled(info))
    return;
  if (WasStartedInForegroundEventInForeground(timing.load_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLoad,
                        timing.load_event_start);
  }
}
