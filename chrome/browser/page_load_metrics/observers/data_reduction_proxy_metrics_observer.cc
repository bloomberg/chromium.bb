// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include <string>

#include "base/time/time.h"
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

namespace {

bool ShouldRecordHistogram(const DataReductionProxyData* data,
                           const base::Optional<base::TimeDelta>& event,
                           const page_load_metrics::PageLoadExtraInfo& info) {
  return data && data->used_data_reduction_proxy() &&
         WasStartedInForegroundOptionalEventInForeground(event, info);
}

// A macro is needed because PAGE_LOAD_HISTOGRAM creates a static instance of
// the histogram. A distinct histogram is needed for each place that calls
// RECORD_HISTOGRAMS_FOR_SUFFIX.
#define RECORD_HISTOGRAMS_FOR_SUFFIX(data, event, info, histogram_suffix)   \
  do {                                                                      \
    if (ShouldRecordHistogram(data.get(), event, info)) {                   \
      PAGE_LOAD_HISTOGRAM(                                                  \
          std::string(internal::kHistogramDataReductionProxyPrefix)         \
              .append(histogram_suffix),                                    \
          event.value());                                                   \
      if (data->lofi_requested()) {                                         \
        PAGE_LOAD_HISTOGRAM(                                                \
            std::string(internal::kHistogramDataReductionProxyLoFiOnPrefix) \
                .append(histogram_suffix),                                  \
            event.value());                                                 \
      }                                                                     \
    }                                                                       \
  } while (false)

}  // namespace

namespace internal {

const char kHistogramDataReductionProxyPrefix[] =
    "PageLoad.Clients.DataReductionProxy.";
const char kHistogramDataReductionProxyLoFiOnPrefix[] =
    "PageLoad.Clients.DataReductionProxy.LoFiOn.";
const char kHistogramDOMContentLoadedEventFiredSuffix[] =
    "DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kHistogramFirstLayoutSuffix[] =
    "DocumentTiming.NavigationToFirstLayout";
const char kHistogramLoadEventFiredSuffix[] =
    "DocumentTiming.NavigationToLoadEventFired";
const char kHistogramFirstContentfulPaintSuffix[] =
    "PaintTiming.NavigationToFirstContentfulPaint";
const char kHistogramFirstImagePaintSuffix[] =
    "PaintTiming.NavigationToFirstImagePaint";
const char kHistogramFirstPaintSuffix[] = "PaintTiming.NavigationToFirstPaint";
const char kHistogramFirstTextPaintSuffix[] =
    "PaintTiming.NavigationToFirstTextPaint";
const char kHistogramParseStartSuffix[] = "ParseTiming.NavigationToParseStart";

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
  if (WasStartedInForegroundOptionalEventInForeground(timing.response_start,
                                                      info)) {
    response_start = timing.response_start.value();
  }
  if (WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                      info)) {
    load_event_start = timing.load_event_start.value();
  }
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_image_paint,
                                                      info)) {
    first_image_paint = timing.first_image_paint.value();
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    first_contentful_paint = timing.first_contentful_paint.value();
  }
  // TODO(ryansturm): Change DataReductionProxyPageLoadTiming to take
  // base::Optional<>s (see crbug.com/626040).
  DataReductionProxyPageLoadTiming data_reduction_proxy_timing(
      timing.navigation_start, response_start, load_event_start,
      first_image_paint, first_contentful_paint);
  GetPingbackClient()->SendPingback(*data_, data_reduction_proxy_timing);
}

void DataReductionProxyMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(
      data_, timing.dom_content_loaded_event_start, info,
      internal::kHistogramDOMContentLoadedEventFiredSuffix);
}

void DataReductionProxyMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, timing.load_event_start, info,
                               internal::kHistogramLoadEventFiredSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstLayout(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, timing.first_layout, info,
                               internal::kHistogramFirstLayoutSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, timing.first_paint, info,
                               internal::kHistogramFirstPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstTextPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, timing.first_text_paint, info,
                               internal::kHistogramFirstTextPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstImagePaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, timing.first_image_paint, info,
                               internal::kHistogramFirstImagePaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, timing.first_contentful_paint, info,
                               internal::kHistogramFirstContentfulPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, timing.parse_start, info,
                               internal::kHistogramParseStartSuffix);
}

DataReductionProxyPingbackClient*
DataReductionProxyMetricsObserver::GetPingbackClient() const {
  return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
             browser_context_)
      ->data_reduction_proxy_service()
      ->pingback_client();
}

}  // namespace data_reduction_proxy
