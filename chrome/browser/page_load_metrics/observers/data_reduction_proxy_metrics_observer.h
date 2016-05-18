// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace content {
class NavigationHandle;
}

namespace page_load_metrics {
struct PageLoadExtraInfo;
struct PageLoadTiming;
}

namespace data_reduction_proxy {

namespace internal {

// Various UMA histogram names for DataReductionProxy core page load metrics.
extern const char kHistogramFirstContentfulPaintDataReductionProxy[];
extern const char kHistogramFirstContentfulPaintDataReductionProxyLoFiOn[];

}  // namespace internal

// Observer responsible for recording core page load metrics releveant to
// DataReductionProxy.
class DataReductionProxyMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DataReductionProxyMetricsObserver();
  ~DataReductionProxyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  // True if the navigation requested LoFi.
  bool lofi_requested_;

  // True if the navigation was proxied through the data reduction proxy.
  bool used_data_reduction_proxy_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserver);
};

}  // namespace data_reduction_proxy

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
