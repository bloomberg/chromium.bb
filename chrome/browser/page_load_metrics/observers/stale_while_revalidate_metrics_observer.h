// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_STALE_WHILE_REVALIDATE_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_STALE_WHILE_REVALIDATE_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace chrome {

class StaleWhileRevalidateMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  StaleWhileRevalidateMetricsObserver();
  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnComplete(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  // True if the committed URL is one of the domains of interest to the
  // stale-while-revalidate experiment.
  bool is_interesting_domain_;

  DISALLOW_COPY_AND_ASSIGN(StaleWhileRevalidateMetricsObserver);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_STALE_WHILE_REVALIDATE_METRICS_OBSERVER_H_
