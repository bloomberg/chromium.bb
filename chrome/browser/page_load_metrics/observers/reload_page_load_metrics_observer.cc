// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/reload_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "ui/base/page_transition_types.h"

namespace chrome {

namespace {

const char kHistogramFirstContentfulPaintReload[] =
    "PageLoad.Clients.Reload.PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramFirstContentfulPaintReloadByGesture[] =
    "PageLoad.Clients.Reload.PaintTiming.NavigationToFirstContentfulPaint."
    "UserGesture";

}  // namespace

ReloadPageLoadMetricsObserver::ReloadPageLoadMetricsObserver()
    : page_transition_was_reload_(false), initiated_by_user_gesture_(false) {}

void ReloadPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  page_transition_was_reload_ = ui::PageTransitionCoreTypeIs(
      navigation_handle->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
  initiated_by_user_gesture_ = navigation_handle->HasUserGesture();
}

void ReloadPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!page_transition_was_reload_ ||
      !WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                               extra_info)) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(kHistogramFirstContentfulPaintReload,
                      timing.first_contentful_paint);
  if (initiated_by_user_gesture_) {
    PAGE_LOAD_HISTOGRAM(kHistogramFirstContentfulPaintReloadByGesture,
                        timing.first_contentful_paint);
  }
}

}  // namespace chrome
