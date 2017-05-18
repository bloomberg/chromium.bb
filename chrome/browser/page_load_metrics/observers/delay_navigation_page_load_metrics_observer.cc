// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/delay_navigation_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"

namespace internal {

const char kHistogramNavigationDelaySpecified[] =
    "PageLoad.Clients.DelayNavigation.Delay.Specified";
const char kHistogramNavigationDelayActual[] =
    "PageLoad.Clients.DelayNavigation.Delay.Actual";
const char kHistogramNavigationDelayDelta[] =
    "PageLoad.Clients.DelayNavigation.Delay.Delta";

}  // namespace internal

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DelayNavigationPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return started_in_foreground ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DelayNavigationPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  return STOP_OBSERVING;
}

void DelayNavigationPageLoadMetricsObserver::OnNavigationDelayComplete(
    base::TimeDelta scheduled_delay,
    base::TimeDelta actual_delay) {
  delay_navigation_ = true;
  scheduled_delay_ = scheduled_delay;
  actual_delay_ = actual_delay;
}

void DelayNavigationPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!delay_navigation_)
    return;

  UMA_HISTOGRAM_TIMES(internal::kHistogramNavigationDelaySpecified,
                      scheduled_delay_);
  UMA_HISTOGRAM_TIMES(internal::kHistogramNavigationDelayActual, actual_delay_);

  base::TimeDelta delay_delta = actual_delay_ - scheduled_delay_;
  UMA_HISTOGRAM_TIMES(internal::kHistogramNavigationDelayDelta,
                      delay_delta.magnitude());
}
