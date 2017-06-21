// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/multi_tab_loading_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace internal {

const char kHistogramMultiTabLoadingFirstContentfulPaint[] =
    "PageLoad.Clients.MultiTabLoading.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramMultiTabLoadingForegroundToFirstContentfulPaint[] =
    "PageLoad.Clients.MultiTabLoading.PaintTiming."
    "ForegroundToFirstContentfulPaint";
const char kHistogramMultiTabLoadingFirstMeaningfulPaint[] =
    "PageLoad.Clients.MultiTabLoading.Experimental.PaintTiming."
    "NavigationToFirstMeaningfulPaint";
const char kHistogramMultiTabLoadingForegroundToFirstMeaningfulPaint[] =
    "PageLoad.Clients.MultiTabLoading.Experimental.PaintTiming."
    "ForegroundToFirstMeaningfulPaint";
const char kHistogramMultiTabLoadingDomContentLoaded[] =
    "PageLoad.Clients.MultiTabLoading.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kBackgroundHistogramMultiTabLoadingDomContentLoaded[] =
    "PageLoad.Clients.MultiTabLoading.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired.Background";
const char kHistogramMultiTabLoadingLoad[] =
    "PageLoad.Clients.MultiTabLoading.DocumentTiming."
    "NavigationToLoadEventFired";
const char kBackgroundHistogramMultiTabLoadingLoad[] =
    "PageLoad.Clients.MultiTabLoading.DocumentTiming."
    "NavigationToLoadEventFired.Background";

}  // namespace internal

MultiTabLoadingPageLoadMetricsObserver::
    MultiTabLoadingPageLoadMetricsObserver() {}

MultiTabLoadingPageLoadMetricsObserver::
    ~MultiTabLoadingPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MultiTabLoadingPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return IsAnyTabLoading(navigation_handle) ? CONTINUE_OBSERVING
                                            : STOP_OBSERVING;
}

void MultiTabLoadingPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramMultiTabLoadingFirstContentfulPaint,
                        timing.paint_timing->first_contentful_paint.value());
  }

  if (WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramMultiTabLoadingForegroundToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value() -
            info.first_foreground_time.value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramMultiTabLoadingFirstMeaningfulPaint,
                        timing.paint_timing->first_meaningful_paint.value());
  }
  if (WasStartedInBackgroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramMultiTabLoadingForegroundToFirstMeaningfulPaint,
        timing.paint_timing->first_meaningful_paint.value() -
            info.first_foreground_time.value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramMultiTabLoadingDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramMultiTabLoadingDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void MultiTabLoadingPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramMultiTabLoadingLoad,
                        timing.document_timing->load_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramMultiTabLoadingLoad,
                        timing.document_timing->load_event_start.value());
  }
}

#if defined(OS_ANDROID)

bool MultiTabLoadingPageLoadMetricsObserver::IsAnyTabLoading(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* this_contents = navigation_handle->GetWebContents();
  for (TabModelList::const_iterator it = TabModelList::begin();
       it != TabModelList::end(); ++it) {
    TabModel* model = *it;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      content::WebContents* other_contents = model->GetWebContentsAt(i);
      if (other_contents && other_contents != this_contents &&
          other_contents->IsLoading()) {
        return true;
      }
    }
  }
  return false;
}

#else  // defined(OS_ANDROID)

bool MultiTabLoadingPageLoadMetricsObserver::IsAnyTabLoading(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* this_contents = navigation_handle->GetWebContents();
  for (auto* browser : *BrowserList::GetInstance()) {
    TabStripModel* model = browser->tab_strip_model();
    for (int i = 0; i < model->count(); ++i) {
      content::WebContents* other_contents = model->GetWebContentsAt(i);
      if (other_contents != this_contents && other_contents->IsLoading()) {
        return true;
      }
    }
  }
  return false;
}

#endif  // defined(OS_ANDROID)
