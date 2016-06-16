// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/renderer_host/chrome_navigation_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

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
    : browser_context_(nullptr) {}

DataReductionProxyMetricsObserver::~DataReductionProxyMetricsObserver() {}

// Check if the NavigationData indicates anything about the DataReductionProxy.
void DataReductionProxyMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // This BrowserContext is valid for the lifetime of
  // DataReductionProxyMetricsObserver. BrowserContext is always valid and
  // non-nullptr in NavigationControllerImpl, which is a member of WebContents.
  // A raw pointer to BrowserContext taken at this point will be valid until
  // after WebContent's destructor. The latest that PageLoadTracker's destructor
  // will be called is in MetricsWebContentsObserver's destrcutor, which is
  // called in WebContents destructor.
  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();
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
  data_ = data->DeepCopy();
}

void DataReductionProxyMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // TODO(ryansturm): Move to OnFirstBackgroundEvent to handle some fast
  // shutdown cases. crbug.com/618072
  if (!browser_context_)
    return;
  if (!data_ || !data_->used_data_reduction_proxy())
    return;
  if (data_reduction_proxy::params::IsIncludedInHoldbackFieldTrial() ||
      data_reduction_proxy::params::IsIncludedInTamperDetectionExperiment()) {
    return;
  }
  // Only consider timing events that happened before the first background
  // event.
  base::TimeDelta response_start;
  base::TimeDelta load_event_start;
  base::TimeDelta first_image_paint;
  base::TimeDelta first_contentful_paint;
  if (WasStartedInForegroundEventInForeground(timing.response_start, info))
    response_start = timing.response_start;
  if (WasStartedInForegroundEventInForeground(timing.load_event_start, info))
    load_event_start = timing.load_event_start;
  if (WasStartedInForegroundEventInForeground(timing.first_image_paint, info))
    first_image_paint = timing.first_image_paint;
  if (WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                              info)) {
    first_contentful_paint = timing.first_contentful_paint;
  }
  DataReductionProxyPageLoadTiming data_reduction_proxy_timing(
      timing.navigation_start, response_start, load_event_start,
      first_image_paint, first_contentful_paint);
  GetPingbackClient()->SendPingback(*data_, data_reduction_proxy_timing);
}

void DataReductionProxyMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!data_ || !data_->used_data_reduction_proxy() ||
      !WasStartedInForegroundEventInForeground(timing.first_contentful_paint,
                                               info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramFirstContentfulPaintDataReductionProxy,
      timing.first_contentful_paint);
  if (!data_->lofi_requested())
    return;
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramFirstContentfulPaintDataReductionProxyLoFiOn,
      timing.first_contentful_paint);
}

DataReductionProxyPingbackClient*
DataReductionProxyMetricsObserver::GetPingbackClient() const {
  return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
             browser_context_)
      ->data_reduction_proxy_service()
      ->pingback_client();
}

}  // namespace data_reduction_proxy
