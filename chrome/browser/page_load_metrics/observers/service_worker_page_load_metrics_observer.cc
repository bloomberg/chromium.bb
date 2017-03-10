// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace internal {

const char kHistogramServiceWorkerParseStart[] =
    "PageLoad.Clients.ServiceWorker.ParseTiming.NavigationToParseStart";
const char kBackgroundHistogramServiceWorkerParseStart[] =
    "PageLoad.Clients.ServiceWorker.ParseTiming.NavigationToParseStart."
    "Background";
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

const char kHistogramServiceWorkerFirstContentfulPaintInbox[] =
    "PageLoad.Clients.ServiceWorker.PaintTiming."
    "NavigationToFirstContentfulPaint.inbox";
const char kHistogramServiceWorkerParseStartToFirstContentfulPaintInbox[] =
    "PageLoad.Clients.ServiceWorker.PaintTiming."
    "ParseStartToFirstContentfulPaint.inbox";
const char kHistogramServiceWorkerDomContentLoadedInbox[] =
    "PageLoad.Clients.ServiceWorker.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired.inbox";
const char kHistogramServiceWorkerLoadInbox[] =
    "PageLoad.Clients.ServiceWorker.DocumentTiming.NavigationToLoadEventFired."
    "inbox";

}  // namespace internal

namespace {

bool IsServiceWorkerControlled(
    const page_load_metrics::PageLoadExtraInfo& info) {
  return (info.main_frame_metadata.behavior_flags &
          blink::WebLoadingBehaviorFlag::
              WebLoadingBehaviorServiceWorkerControlled) != 0;
}

bool IsInboxSite(const GURL& url) {
  return url.host_piece() == "inbox.google.com";
}

}  // namespace

ServiceWorkerPageLoadMetricsObserver::ServiceWorkerPageLoadMetricsObserver() {}

void ServiceWorkerPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!IsServiceWorkerControlled(info))
    return;
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramServiceWorkerFirstContentfulPaint,
        timing.first_contentful_paint.value());
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerFirstContentfulPaint,
                      timing.first_contentful_paint.value());
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramServiceWorkerParseStartToFirstContentfulPaint,
      timing.first_contentful_paint.value() - timing.parse_start.value());

  if (IsInboxSite(info.url)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerFirstContentfulPaintInbox,
        timing.first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramServiceWorkerParseStartToFirstContentfulPaintInbox,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!IsServiceWorkerControlled(info))
    return;
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerDomContentLoaded,
                      timing.dom_content_loaded_event_start.value());
  if (IsInboxSite(info.url)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerDomContentLoadedInbox,
                        timing.dom_content_loaded_event_start.value());
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!IsServiceWorkerControlled(info))
    return;
  if (!WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                       info))
    return;
  PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLoad,
                      timing.load_event_start.value());
  if (IsInboxSite(info.url)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerLoadInbox,
                        timing.load_event_start.value());
  }
}

void ServiceWorkerPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!IsServiceWorkerControlled(info))
    return;
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_start,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramServiceWorkerParseStart,
                        timing.parse_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramServiceWorkerParseStart,
                        timing.parse_start.value());
  }
}
