// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_

#include <stdint.h>

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
extern const char kHistogramFirstMeaningfulPaintSuffix[];
extern const char kHistogramFirstImagePaintSuffix[];
extern const char kHistogramFirstPaintSuffix[];
extern const char kHistogramFirstTextPaintSuffix[];
extern const char kHistogramParseStartSuffix[];
extern const char kHistogramParseBlockedOnScriptLoadSuffix[];
extern const char kHistogramParseDurationSuffix[];

// Byte and request specific histogram suffixes.
extern const char kResourcesPercentProxied[];
extern const char kBytesPercentProxied[];
extern const char kBytesCompressionRatio[];
extern const char kBytesInflationPercent[];
extern const char kNetworkResources[];
extern const char kResourcesProxied[];
extern const char kResourcesNotProxied[];
extern const char kNetworkBytes[];
extern const char kBytesProxied[];
extern const char kBytesNotProxied[];
extern const char kBytesOriginal[];
extern const char kBytesSavings[];
extern const char kBytesInflation[];

}  // namespace internal

// Observer responsible for recording core page load metrics releveant to
// DataReductionProxy.
class DataReductionProxyMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  DataReductionProxyMetricsObserver();
  ~DataReductionProxyMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
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
  void OnFirstMeaningfulPaint(
      const page_load_metrics::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnParseStart(const page_load_metrics::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnParseStop(const page_load_metrics::PageLoadTiming& timing,
                   const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnLoadedResource(
      const page_load_metrics::ExtraRequestInfo& extra_request_info) override;

 private:
  // Sends the page load information to the pingback client.
  void SendPingback(const page_load_metrics::PageLoadTiming& timing,
                    const page_load_metrics::PageLoadExtraInfo& info);

  // Records UMA of page size when the observer is about to be deleted.
  void RecordPageSizeUMA() const;

  // Gets the default DataReductionProxyPingbackClient. Overridden in testing.
  virtual DataReductionProxyPingbackClient* GetPingbackClient() const;

  // Data related to this navigation.
  std::unique_ptr<DataReductionProxyData> data_;

  // The browser context this navigation is operating in.
  content::BrowserContext* browser_context_;

  // The number of resources that used data reduction proxy.
  int num_data_reduction_proxy_resources_;

  // The number of resources that did not come from cache.
  int num_network_resources_;

  // The total content network bytes that the user would have downloaded if they
  // were not using data reduction proxy.
  int64_t original_network_bytes_;

  // The total network bytes loaded through data reduction proxy.
  int64_t network_bytes_proxied_;

  // The total network bytes used.
  int64_t network_bytes_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserver);
};

}  // namespace data_reduction_proxy

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_DATA_REDUCTION_PROXY_METRICS_OBSERVER_H_
