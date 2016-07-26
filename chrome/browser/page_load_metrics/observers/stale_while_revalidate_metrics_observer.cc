// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/stale_while_revalidate_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/stale_while_revalidate_experiment_domains.h"

namespace chrome {

StaleWhileRevalidateMetricsObserver::StaleWhileRevalidateMetricsObserver()
    : is_interesting_domain_(false) {}

void StaleWhileRevalidateMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  is_interesting_domain_ = net::IsHostInStaleWhileRevalidateExperimentDomain(
      navigation_handle->GetURL().host());
}

void StaleWhileRevalidateMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  using page_load_metrics::WasStartedInForegroundOptionalEventInForeground;

  if (!is_interesting_domain_)
    return;

  if (WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                      extra_info)) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.StaleWhileRevalidateExperiment.Timing2."
        "NavigationToLoadEventFired",
        timing.load_event_start.value());
  }
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_layout,
                                                      extra_info)) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.StaleWhileRevalidateExperiment.Timing2."
        "NavigationToFirstLayout",
        timing.first_layout.value());
  }
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_text_paint,
                                                      extra_info)) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.StaleWhileRevalidateExperiment.Timing2."
        "NavigationToFirstTextPaint",
        timing.first_text_paint.value());
  }
}

}  // namespace chrome
