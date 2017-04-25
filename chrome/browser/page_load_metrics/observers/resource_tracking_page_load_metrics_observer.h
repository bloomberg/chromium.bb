// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_TRACKING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_TRACKING_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace page_load_metrics {

class ResourceTrackingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ResourceTrackingPageLoadMetricsObserver();
  ~ResourceTrackingPageLoadMetricsObserver() override;

  // Called by the PageLoadMetrics framework when we start a new request, so we
  // can update our data structures to be able to calculate a resource done
  // percentage.
  void OnStartedResource(
      const ExtraRequestStartInfo& extra_request_start_info) override;

  // Called by the PageLoadMetrics framework when we start a new request, so we
  // can update our data structures to be able to calculate a resource done
  // percentage.
  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) override;

  // For the specified type, get the count of requests started and completed.
  // TODO(petewil) Note that type is not used yet, code to use it is coming
  // soon.
  void GetCountsForTypeForTesting(const content::ResourceType type,
                                  int64_t* started_count,
                                  int64_t* completed_count);

 private:
  // TODO(petewil): Some way to keep track of what we've seen
  // TODO(petewil): Some way to inform our keeper of aggregate results when they
  // change.
  int64_t started_count_;
  int64_t completed_count_;
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_RESOURCE_TRACKING_PAGE_LOAD_METRICS_OBSERVER_H_
