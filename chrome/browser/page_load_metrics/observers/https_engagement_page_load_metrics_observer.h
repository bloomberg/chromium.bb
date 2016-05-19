// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"

namespace internal {
extern const char kHttpsEngagementHistogram[];
extern const char kHttpEngagementHistogram[];
}  // namespace internal

class HttpsEngagementPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  HttpsEngagementPageLoadMetricsObserver();

  // page_load_metrics::PageLoadMetricsObserver:
  void OnStart(content::NavigationHandle* navigation_handle,
               const GURL& currently_committed_url,
               bool started_in_foreground) override;
  void OnHidden() override;
  void OnShown() override;
  void OnComplete(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  bool currently_in_foreground_;
  base::TimeDelta foreground_time_;
  base::TimeTicks last_time_shown_;

  DISALLOW_COPY_AND_ASSIGN(HttpsEngagementPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_PAGE_LOAD_METRICS_OBSERVER_H_
