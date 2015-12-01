// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/stale_while_revalidate_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/stale_while_revalidate_experiment_domains.h"

namespace chrome {

StaleWhileRevalidateMetricsObserver::StaleWhileRevalidateMetricsObserver(
    page_load_metrics::PageLoadMetricsObservable* metrics)
    : is_interesting_domain_(false), metrics_(metrics) {}

void StaleWhileRevalidateMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  is_interesting_domain_ = net::IsHostInStaleWhileRevalidateExperimentDomain(
      navigation_handle->GetURL().host());
}

void StaleWhileRevalidateMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!is_interesting_domain_)
    return;

  if (!timing.load_event_start.is_zero()) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.StaleWhileRevalidateExperiment.Timing2."
        "NavigationToLoadEventFired",
        timing.load_event_start);
  }
  if (!timing.first_layout.is_zero()) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.StaleWhileRevalidateExperiment.Timing2."
        "NavigationToFirstLayout",
        timing.first_layout);
  }
  if (!timing.first_text_paint.is_zero()) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.StaleWhileRevalidateExperiment.Timing2."
        "NavigationToFirstTextPaint",
        timing.first_text_paint);
  }
}

void StaleWhileRevalidateMetricsObserver::OnPageLoadMetricsGoingAway() {
  metrics_->RemoveObserver(this);
  delete this;
}

}  // namespace chrome
