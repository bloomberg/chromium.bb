// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/css_scanning_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

CssScanningMetricsObserver::CssScanningMetricsObserver() {}

CssScanningMetricsObserver::~CssScanningMetricsObserver() {}

void CssScanningMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.metadata.behavior_flags &
          blink::WebLoadingBehaviorFlag::WebLoadingBehaviorCSSPreloadFound &&
      WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.CssScanner.PaintTiming."
        "ParseStartToFirstContentfulPaint",
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }
}

void CssScanningMetricsObserver::OnFirstMeaningfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (info.metadata.behavior_flags &
          blink::WebLoadingBehaviorFlag::WebLoadingBehaviorCSSPreloadFound &&
      WasStartedInForegroundOptionalEventInForeground(
          timing.first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.CssScanner.Experimental.PaintTiming."
        "ParseStartToFirstMeaningfulPaint",
        timing.first_meaningful_paint.value() - timing.parse_start.value());
  }
}
