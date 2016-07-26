// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"

namespace content {
class BrowserContext;
class NavigationHandle;
}

namespace page_load_metrics {
struct PageLoadExtraInfo;
struct PageLoadTiming;
}

namespace data_reduction_proxy {
class DataReductionProxyData;
class DataReductionProxyPingbackClient;

namespace internal {

// Various UMA histogram names for DataReductionProxy core page load metrics.
extern const char kHistogramDataReductionProxyPrefix[];
extern const char kHistogramDataReductionProxyLoFiOnPrefix[];
extern const char kHistogramDOMContentLoadedEventFiredSuffix[];
extern const char kHistogramFirstLayoutSuffix[];
extern const char kHistogramLoadEventFiredSuffix[];
extern const char kHistogramFirstContentfulPaintSuffix[];
extern const char kHistogramFirstImagePaintSuffix[];
extern const char kHistogramFirstPaintSuffix[];
extern const char kHistogramFirstTextPaintSuffix[];
extern const char kHistogramParseStartSuffix[];

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
  void OnComplete(const page_load_metrics::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadEventStart(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstLayout(const page_load_metrics::PageLoadTiming& timing,
                     const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstPaint(const page_load_metrics::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstTextPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstImagePaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnFirstContentfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnParseStart(const page_load_metrics::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info) override;

 private:
  // Gets the default DataReductionProxyPingbackClient. Overridden in testing.
  virtual DataReductionProxyPingbackClient* GetPingbackClient() const;

  // Data related to this navigation.
  std::unique_ptr<DataReductionProxyData> data_;

  // The browser context this navigation is operating in.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserver);
};

}  // namespace data_reduction_proxy

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
