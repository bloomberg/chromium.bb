// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_

#include <bitset>
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"

namespace internal {

const char kFeaturesHistogramName[] =
    "Blink.UseCounter.Features_TestBrowserProcessLogging";

}  // namespace internal

class UseCounterPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  UseCounterPageLoadMetricsObserver();
  ~UseCounterPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver.
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnFeaturesUsageObserved(
      const page_load_metrics::mojom::PageLoadFeatures&,
      const page_load_metrics::PageLoadExtraInfo& extra_info) override;

 private:
  // To keep tracks of which features have been measured.
  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      features_recorded_;
  DISALLOW_COPY_AND_ASSIGN(UseCounterPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
