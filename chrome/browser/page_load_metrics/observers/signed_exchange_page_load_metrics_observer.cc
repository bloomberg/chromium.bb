// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/signed_exchange_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"

namespace internal {

#define HISTOGRAM_PREFIX "PageLoad.Clients.SignedExchange."

constexpr char kHistogramSignedExchangePrefix[] = HISTOGRAM_PREFIX;
constexpr char kHistogramSignedExchangeParseStart[] =
    HISTOGRAM_PREFIX "ParseTiming.NavigationToParseStart";
constexpr char kHistogramSignedExchangeFirstInputDelay[] =
    HISTOGRAM_PREFIX "InteractiveTiming.FirstInputDelay";
constexpr char kHistogramSignedExchangeFirstPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstPaint";
constexpr char kHistogramSignedExchangeFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.NavigationToFirstContentfulPaint";
constexpr char kHistogramSignedExchangeParseStartToFirstContentfulPaint[] =
    HISTOGRAM_PREFIX "PaintTiming.ParseStartToFirstContentfulPaint";
constexpr char kHistogramSignedExchangeFirstMeaningfulPaint[] = HISTOGRAM_PREFIX
    "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
constexpr char kHistogramSignedExchangeParseStartToFirstMeaningfulPaint[] =
    HISTOGRAM_PREFIX
    "Experimental.PaintTiming.ParseStartToFirstMeaningfulPaint";
constexpr char kHistogramSignedExchangeDomContentLoaded[] =
    HISTOGRAM_PREFIX "DocumentTiming.NavigationToDOMContentLoadedEventFired";
constexpr char kHistogramSignedExchangeLoad[] =
    HISTOGRAM_PREFIX "DocumentTiming.NavigationToLoadEventFired";

#undef HISTOGRAM_PREFIX

}  // namespace internal

SignedExchangePageLoadMetricsObserver::SignedExchangePageLoadMetricsObserver() {
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SignedExchangePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  if (navigation_handle->IsSignedExchangeInnerResponse())
    return CONTINUE_OBSERVING;

  return STOP_OBSERVING;
}

void SignedExchangePageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, info)) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramSignedExchangeFirstPaint,
                      timing.paint_timing->first_paint.value());
}

void SignedExchangePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramSignedExchangeFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramSignedExchangeParseStartToFirstContentfulPaint,
      timing.paint_timing->first_contentful_paint.value() -
          timing.parse_timing->parse_start.value());
}

void SignedExchangePageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramSignedExchangeFirstMeaningfulPaint,
                      timing.paint_timing->first_meaningful_paint.value());
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramSignedExchangeParseStartToFirstMeaningfulPaint,
      timing.paint_timing->first_meaningful_paint.value() -
          timing.parse_timing->parse_start.value());
}

void SignedExchangePageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start, info)) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramSignedExchangeDomContentLoaded,
      timing.document_timing->dom_content_loaded_event_start.value());
}

void SignedExchangePageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramSignedExchangeLoad,
                      timing.document_timing->load_event_start.value());
}

void SignedExchangePageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, info)) {
    return;
  }

  // Copied from the CorePageLoadMetricsObserver implementation.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      internal::kHistogramSignedExchangeFirstInputDelay,
      timing.interactive_timing->first_input_delay.value(),
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(60),
      50);
}

void SignedExchangePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, info)) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(internal::kHistogramSignedExchangeParseStart,
                      timing.parse_timing->parse_start.value());
}
