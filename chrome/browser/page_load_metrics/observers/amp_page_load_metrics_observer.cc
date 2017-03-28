// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/amp_page_load_metrics_observer.h"

#include <string>

#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace {

const char kHistogramAMPDOMContentLoadedEventFired[] =
    "PageLoad.Clients.AMPCache2.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramAMPFirstLayout[] =
    "PageLoad.Clients.AMPCache2.DocumentTiming.NavigationToFirstLayout";
const char kHistogramAMPLoadEventFired[] =
    "PageLoad.Clients.AMPCache2.DocumentTiming.NavigationToLoadEventFired";
const char kHistogramAMPFirstContentfulPaint[] =
    "PageLoad.Clients.AMPCache2.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramAMPParseStart[] =
    "PageLoad.Clients.AMPCache2.ParseTiming.NavigationToParseStart";

// Host pattern for AMP Cache URLs.
// See https://developers.google.com/amp/cache/overview#amp-cache-url-format
// for a definition of the format of AMP Cache URLs.
const char kAmpCacheHost[] = "cdn.ampproject.org";

// Pattern for the path of Google AMP Viewer URLs.
const char kGoogleAmpViewerPathPattern[] = "/amp/";

bool IsAMPCacheURL(const GURL& url) {
  return url.host() == kAmpCacheHost ||
         (google_util::IsGoogleDomainUrl(
              url, google_util::DISALLOW_SUBDOMAIN,
              google_util::DISALLOW_NON_STANDARD_PORTS) &&
          base::StartsWith(url.path(), kGoogleAmpViewerPathPattern,
                           base::CompareCase::SENSITIVE));
}

}  // namespace

AMPPageLoadMetricsObserver::AMPPageLoadMetricsObserver() {}

AMPPageLoadMetricsObserver::~AMPPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AMPPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  return IsAMPCacheURL(navigation_handle->GetURL()) ? CONTINUE_OBSERVING
                                                    : STOP_OBSERVING;
}

void AMPPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(kHistogramAMPDOMContentLoadedEventFired,
                      timing.dom_content_loaded_event_start.value());
}

void AMPPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                       info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(kHistogramAMPLoadEventFired,
                      timing.load_event_start.value());
}

void AMPPageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(timing.first_layout,
                                                       info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(kHistogramAMPFirstLayout, timing.first_layout.value());
}

void AMPPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(kHistogramAMPFirstContentfulPaint,
                      timing.first_contentful_paint.value());
}

void AMPPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(timing.parse_start,
                                                       info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(kHistogramAMPParseStart, timing.parse_start.value());
}
