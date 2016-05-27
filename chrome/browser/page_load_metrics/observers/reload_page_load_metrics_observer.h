// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RELOAD_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RELOAD_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace chrome {

// An observer class to report metrics on pages those loading were initiated
// by one of reload operations including hitting enter in the omnibox.
// This ignore JavaScript-initiated loadings by location.reload() or
// history.go(0).
class ReloadPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ReloadPageLoadMetricsObserver();

  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  bool page_transition_was_reload_;
  bool initiated_by_user_gesture_;

  DISALLOW_COPY_AND_ASSIGN(ReloadPageLoadMetricsObserver);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RELOAD_PAGE_LOAD_METRICS_OBSERVER_H_
