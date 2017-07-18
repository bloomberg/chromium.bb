// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/use_counter_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"

using WebFeature = blink::mojom::WebFeature;
using Features = page_load_metrics::mojom::PageLoadFeatures;

UseCounterPageLoadMetricsObserver::UseCounterPageLoadMetricsObserver() {}
UseCounterPageLoadMetricsObserver::~UseCounterPageLoadMetricsObserver() {}

void UseCounterPageLoadMetricsObserver::OnFeaturesUsageObserved(
    const Features& features) {
  for (auto feature : features.features) {
#if DCHECK_IS_ON()
    // The usage of each feature should be only measured once. The renderer side
    // is reponsible for only sending the usage of each feature once through
    // IPC. Use DCHECK here for validation.
    DCHECK(!features_recorded_.test(static_cast<size_t>(feature)));
    features_recorded_.set(static_cast<size_t>(feature));
#endif
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.UseCounter.Features_TestBrowserProcessLogging", feature,
        WebFeature::kNumberOfFeatures);
  }
}
