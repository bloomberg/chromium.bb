// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#endif  // defined(OS_ANDROID)

namespace previews {

namespace internal {

const char kHistogramOfflinePreviewsDOMContentLoadedEventFired[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramOfflinePreviewsFirstLayout[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToFirstLayout";
const char kHistogramOfflinePreviewsLoadEventFired[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToLoadEventFired";
const char kHistogramOfflinePreviewsFirstContentfulPaint[] =
    "PageLoad.Clients.Previews.OfflinePages.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramOfflinePreviewsParseStart[] =
    "PageLoad.Clients.Previews.OfflinePages.ParseTiming.NavigationToParseStart";

}  // namespace internal

PreviewsPageLoadMetricsObserver::PreviewsPageLoadMetricsObserver() {}

PreviewsPageLoadMetricsObserver::~PreviewsPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  return IsOfflinePreview(navigation_handle->GetWebContents())
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  // On top of base-supported types, support MHTML. Offline previews are served
  // as MHTML (multipart/related).
  return PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
                     CONTINUE_OBSERVING ||
                 mime_type == "multipart/related"
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

void PreviewsPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramOfflinePreviewsDOMContentLoadedEventFired,
      timing.dom_content_loaded_event_start.value());
}

void PreviewsPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                       info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsLoadEventFired,
                      timing.load_event_start.value());
}

void PreviewsPageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(timing.first_layout,
                                                       info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsFirstLayout,
                      timing.first_layout.value());
}

void PreviewsPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsFirstContentfulPaint,
                      timing.first_contentful_paint.value());
}

void PreviewsPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(timing.parse_start,
                                                       info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsParseStart,
                      timing.parse_start.value());
}

bool PreviewsPageLoadMetricsObserver::IsOfflinePreview(
    content::WebContents* web_contents) const {
#if defined(OS_ANDROID)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->IsShowingOfflinePreview();
#else
  return false;
#endif  // defined(OS_ANDROID)
}

}  // namespace previews
