// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace content {
class NavigationHandle;
}

namespace page_load_metrics {
struct PageLoadExtraInfo;
struct PageLoadTiming;
}

// Observer responsible for recording page load metrics relevant to page served
// from the AMP cache. When AMP pages are served in a same page navigation, UMA
// is not recorded; this is typical for AMP pages navigated to from google.com.
// Navigations to AMP pages from the google search app or directly to the amp
// cache page will be tracked. Refreshing an AMP page served from google.com
// will be tracked.
class AMPPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AMPPageLoadMetricsObserver();
  ~AMPPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstLayout(const page_load_metrics::PageLoadTiming& timing,
                     const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnParseStart(const page_load_metrics::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AMPPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_
