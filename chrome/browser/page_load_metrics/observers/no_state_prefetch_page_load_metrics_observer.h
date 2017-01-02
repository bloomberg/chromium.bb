// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NO_STATE_PREFETCH_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NO_STATE_PREFETCH_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace prerender {
class PrerenderManager;
}

// Observer recording metrics related to NoStatePrefetch.
class NoStatePrefetchPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a NoStatePrefetchPageLoadMetricsObserver, or nullptr if it is not
  // needed.
  static std::unique_ptr<NoStatePrefetchPageLoadMetricsObserver> CreateIfNeeded(
      content::WebContents* web_contents);

  explicit NoStatePrefetchPageLoadMetricsObserver(
      prerender::PrerenderManager* manager);
  ~NoStatePrefetchPageLoadMetricsObserver() override;

 private:
  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  ObservePolicy OnHidden(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

  bool is_no_store_;  // True if the main resource has a "no-store" HTTP header.
  bool was_hidden_;   // The page went to background while rendering.
  prerender::PrerenderManager* const prerender_manager_;

  DISALLOW_COPY_AND_ASSIGN(NoStatePrefetchPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NO_STATE_PREFETCH_PAGE_LOAD_METRICS_OBSERVER_H_
