// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include "chrome/browser/renderer_host/chrome_navigation_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_handle.h"

namespace data_reduction_proxy {

namespace internal {

const char kHistogramFirstContentfulPaintDataReductionProxy[] =
    "PageLoad.Clients.DataReductionProxy.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramFirstContentfulPaintDataReductionProxyLoFiOn[] =
    "PageLoad.Clients.DataReductionProxy.LoFiOn.PaintTiming."
    "NavigationToFirstContentfulPaint";

}  // namespace internal

DataReductionProxyMetricsObserver::DataReductionProxyMetricsObserver()
    : lofi_requested_(false), used_data_reduction_proxy_(false) {}

DataReductionProxyMetricsObserver::~DataReductionProxyMetricsObserver() {}

// Check if the NavigationData indicates anything about the DataReductionProxy.
void DataReductionProxyMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // As documented in content/public/browser/navigation_handle.h, this
  // NavigationData is a clone of the NavigationData instance returned from
  // ResourceDispatcherHostDelegate::GetNavigationData during commit.
  // Because ChromeResourceDispatcherHostDelegate always returns a
  // ChromeNavigationData, it is safe to static_cast here.
  ChromeNavigationData* chrome_navigation_data =
      static_cast<ChromeNavigationData*>(
          navigation_handle->GetNavigationData());
  if (!chrome_navigation_data)
    return;
  data_reduction_proxy::DataReductionProxyData* data =
      chrome_navigation_data->GetDataReductionProxyData();
  if (!data)
    return;
  used_data_reduction_proxy_ = data->used_data_reduction_proxy();
  lofi_requested_ = data->lofi_requested();
}

void DataReductionProxyMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!used_data_reduction_proxy_ ||
      !WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                               info))
    return;
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramFirstContentfulPaintDataReductionProxy,
      timing.first_contentful_paint);
  if (!lofi_requested_)
    return;
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramFirstContentfulPaintDataReductionProxyLoFiOn,
      timing.first_contentful_paint);
}

}  // namespace data_reduction_proxy
