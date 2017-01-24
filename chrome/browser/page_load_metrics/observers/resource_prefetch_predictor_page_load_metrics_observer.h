// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_PREFETCH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_PREFETCH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace content {
class WebContents;
}

namespace predictors {
class ResourcePrefetchPredictor;
}

namespace internal {

extern const char kHistogramResourcePrefetchPredictorFirstContentfulPaint[];
extern const char kHistogramResourcePrefetchPredictorFirstMeaningfulPaint[];

}  // namespace internal

// Observer responsible for recording page load metrics relevant to
// ResourcePrefetchPredictor.
class ResourcePrefetchPredictorPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a ResourcePrefetchPredictorPageLoadMetricsObserver, or nullptr if
  // it is not needed.
  static std::unique_ptr<ResourcePrefetchPredictorPageLoadMetricsObserver>
  CreateIfNeeded(content::WebContents* web_contents);

  // Public for testing. Normally one should use CreateIfNeeded. Predictor must
  // outlive this observer.
  explicit ResourcePrefetchPredictorPageLoadMetricsObserver(
      predictors::ResourcePrefetchPredictor* predictor);

  ~ResourcePrefetchPredictorPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_commited_url,
                        bool started_in_foreground) override;
  ObservePolicy OnHidden(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;
  void OnFirstMeaningfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  predictors::ResourcePrefetchPredictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_PREFETCH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
