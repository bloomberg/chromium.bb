// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"
#if DCHECK_IS_ON()
#include <bitset>
#endif

class UseCounterPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  UseCounterPageLoadMetricsObserver();
  ~UseCounterPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver.
  void OnFeaturesUsageObserved(
      const page_load_metrics::mojom::PageLoadFeatures&) override;

 private:
#if DCHECK_IS_ON()
  // To keep tracks of which features have been measured.
  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      features_recorded_;
#endif
  DISALLOW_COPY_AND_ASSIGN(UseCounterPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
