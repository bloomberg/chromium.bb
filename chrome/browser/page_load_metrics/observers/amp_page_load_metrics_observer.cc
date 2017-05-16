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

using AMPViewType = AMPPageLoadMetricsObserver::AMPViewType;

const char kHistogramPrefix[] = "PageLoad.Clients.AMP.";

const char kHistogramAMPDOMContentLoadedEventFired[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kHistogramAMPFirstLayout[] =
    "DocumentTiming.NavigationToFirstLayout";
const char kHistogramAMPLoadEventFired[] =
    "DocumentTiming.NavigationToLoadEventFired";
const char kHistogramAMPFirstContentfulPaint[] =
    "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramAMPParseStart[] = "ParseTiming.NavigationToParseStart";

// Host pattern for AMP Cache URLs.
// See https://developers.google.com/amp/cache/overview#amp-cache-url-format
// for a definition of the format of AMP Cache URLs.
const char kAmpCacheHostSuffix[] = "cdn.ampproject.org";

#define RECORD_HISTOGRAM_FOR_TYPE(name, amp_view_type, value)                 \
  do {                                                                        \
    PAGE_LOAD_HISTOGRAM(std::string(kHistogramPrefix).append(name), value);   \
    switch (amp_view_type) {                                                  \
      case AMPViewType::AMP_CACHE:                                            \
        PAGE_LOAD_HISTOGRAM(                                                  \
            std::string(kHistogramPrefix).append("AmpCache.").append(name),   \
            value);                                                           \
        break;                                                                \
      case AMPViewType::GOOGLE_SEARCH_AMP_VIEWER:                             \
        PAGE_LOAD_HISTOGRAM(std::string(kHistogramPrefix)                     \
                                .append("GoogleSearch.")                      \
                                .append(name),                                \
                            value);                                           \
        break;                                                                \
      case AMPViewType::GOOGLE_NEWS_AMP_VIEWER:                               \
        PAGE_LOAD_HISTOGRAM(                                                  \
            std::string(kHistogramPrefix).append("GoogleNews.").append(name), \
            value);                                                           \
        break;                                                                \
      case AMPViewType::NONE:                                                 \
        NOTREACHED();                                                         \
        break;                                                                \
    }                                                                         \
  } while (false)

}  // namespace

AMPPageLoadMetricsObserver::AMPPageLoadMetricsObserver() {}

AMPPageLoadMetricsObserver::~AMPPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AMPPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  view_type_ = GetAMPViewType(navigation_handle->GetURL());
  return (view_type_ != AMPViewType::NONE) ? CONTINUE_OBSERVING
                                           : STOP_OBSERVING;
}

void AMPPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing.dom_content_loaded_event_start, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(
      kHistogramAMPDOMContentLoadedEventFired, view_type_,
      timing.document_timing.dom_content_loaded_event_start.value());
}

void AMPPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing.load_event_start, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPLoadEventFired, view_type_,
                            timing.document_timing.load_event_start.value());
}

void AMPPageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing.first_layout, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPFirstLayout, view_type_,
                            timing.document_timing.first_layout.value());
}

void AMPPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing.first_contentful_paint, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPFirstContentfulPaint, view_type_,
                            timing.paint_timing.first_contentful_paint.value());
}

void AMPPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing.parse_start, info)) {
    return;
  }
  RECORD_HISTOGRAM_FOR_TYPE(kHistogramAMPParseStart, view_type_,
                            timing.parse_timing.parse_start.value());
}

// static
AMPPageLoadMetricsObserver::AMPViewType
AMPPageLoadMetricsObserver::GetAMPViewType(const GURL& url) {
  if (base::EndsWith(url.host(), kAmpCacheHostSuffix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return AMPViewType::AMP_CACHE;
  }

  base::Optional<std::string> google_hostname_prefix =
      page_load_metrics::GetGoogleHostnamePrefix(url);
  if (!google_hostname_prefix.has_value())
    return AMPViewType::NONE;

  if (google_hostname_prefix.value() == "www" &&
      base::StartsWith(url.path_piece(), "/amp/",
                       base::CompareCase::SENSITIVE)) {
    return AMPViewType::GOOGLE_SEARCH_AMP_VIEWER;
  }

  if (google_hostname_prefix.value() == "news" &&
      url.path_piece() == "/news/amp") {
    return AMPViewType::GOOGLE_NEWS_AMP_VIEWER;
  }
  return AMPViewType::NONE;
}
