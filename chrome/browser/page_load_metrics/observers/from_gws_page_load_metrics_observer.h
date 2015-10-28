// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

class FromGWSPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit FromGWSPageLoadMetricsObserver(
      page_load_metrics::PageLoadMetricsObservable* metrics);
  // page_load_metrics::PageLoadMetricsObserver implementation:
  void OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnComplete(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnPageLoadMetricsGoingAway() override;

 protected:
  // Called in tests.
  void SetCommittedURLAndReferrer(const GURL& url,
                                  const content::Referrer& referrer);

 private:
  bool navigation_from_gws_;
  page_load_metrics::PageLoadMetricsObservable* const metrics_;

  DISALLOW_COPY_AND_ASSIGN(FromGWSPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FROM_GWS_PAGE_LOAD_METRICS_OBSERVER_H_
