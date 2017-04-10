// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DELAY_NAVIGATION_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DELAY_NAVIGATION_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramNavigationDelaySpecified[];
extern const char kHistogramNavigationDelayActual[];
extern const char kHistogramNavigationDelayDelta[];

}  // namespace internal

// DelayNavigationPageLoadMetricsObserver tracks delays in navigations
// introduced by the DelayNavigation experiment. This class is temporary and
// will be removed once the experiment is complete.
//
// Metrics logged by this class complement the metrics logged in
// DelayNavigationThrottle. DelayNavigationThrottle metrics are logged for all
// main frame loads, including those that happen in the background and thus may
// be throttled, whereas this observer logs the same metrics for page loads
// tracked by the page load metrics infrastructure. We are most interested in
// impact on foregrounded page loads, so we use
// DelayNavigationPageLoadMetricsObserver to log metrics for just those
// pageloads that happen in the foreground.
class DelayNavigationPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DelayNavigationPageLoadMetricsObserver() = default;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnHidden(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnNavigationDelayComplete(base::TimeDelta scheduled_delay,
                                 base::TimeDelta actual_delay) override;
  void OnFirstPaint(const page_load_metrics::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  bool delay_navigation_ = false;
  base::TimeDelta scheduled_delay_;
  base::TimeDelta actual_delay_;

  DISALLOW_COPY_AND_ASSIGN(DelayNavigationPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DELAY_NAVIGATION_PAGE_LOAD_METRICS_OBSERVER_H_
