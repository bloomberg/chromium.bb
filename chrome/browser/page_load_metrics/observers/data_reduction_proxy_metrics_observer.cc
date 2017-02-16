// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

// Appends |suffix| to |kHistogramDataReductionProxyPrefix| and returns it as a
// string.
std::string GetConstHistogramWithSuffix(const char* suffix) {
  return std::string(internal::kHistogramDataReductionProxyPrefix)
      .append(suffix);
}

// A macro is needed because PAGE_LOAD_HISTOGRAM creates a static instance of
// the histogram. A distinct histogram is needed for each place that calls
// RECORD_HISTOGRAMS_FOR_SUFFIX. |event| is the timing event representing when
// |value| became available.
#define RECORD_HISTOGRAMS_FOR_SUFFIX(data, value, histogram_suffix)            \
  do {                                                                         \
    PAGE_LOAD_HISTOGRAM(GetConstHistogramWithSuffix(histogram_suffix), value); \
    if (data->lofi_requested()) {                                              \
      PAGE_LOAD_HISTOGRAM(                                                     \
          std::string(internal::kHistogramDataReductionProxyLoFiOnPrefix)      \
              .append(histogram_suffix),                                       \
          value);                                                              \
    }                                                                          \
  } while (false)

// Like RECORD_HISTOGRAMS_FOR_SUFFIX, but only records histograms if the event
// occurred while the page was in the foreground.
#define RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(info, data, timing,         \
                                                histogram_suffix)           \
  do {                                                                      \
    if (WasStartedInForegroundOptionalEventInForeground(timing, info)) {    \
      RECORD_HISTOGRAMS_FOR_SUFFIX(data, timing.value(), histogram_suffix); \
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
const char kHistogramFirstMeaningfulPaintSuffix[] =
    "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kHistogramFirstImagePaintSuffix[] =
    "PaintTiming.NavigationToFirstImagePaint";
const char kHistogramFirstPaintSuffix[] = "PaintTiming.NavigationToFirstPaint";
const char kHistogramFirstTextPaintSuffix[] =
    "PaintTiming.NavigationToFirstTextPaint";
const char kHistogramParseStartSuffix[] = "ParseTiming.NavigationToParseStart";
const char kHistogramParseBlockedOnScriptLoadSuffix[] =
    "ParseTiming.ParseBlockedOnScriptLoad";
const char kHistogramParseDurationSuffix[] = "ParseTiming.ParseDuration";

const char kResourcesPercentProxied[] =
    "Experimental.CompletedResources.Network.PercentProxied";
const char kBytesPercentProxied[] = "Experimental.Bytes.Network.PercentProxied";
const char kBytesCompressionRatio[] =
    "Experimental.Bytes.Network.CompressionRatio";
const char kBytesInflationPercent[] =
    "Experimental.Bytes.Network.InflationPercent";
const char kNetworkResources[] = "Experimental.CompletedResources.Network";
const char kResourcesProxied[] =
    "Experimental.CompletedResources.Network.Proxied";
const char kResourcesNotProxied[] =
    "Experimental.CompletedResources.Network.NonProxied";
const char kNetworkBytes[] = "Experimental.Bytes.Network";
const char kBytesProxied[] = "Experimental.Bytes.Network.Proxied";
const char kBytesNotProxied[] = "Experimental.Bytes.Network.NonProxied";
const char kBytesOriginal[] = "Experimental.Bytes.Network.Original";
const char kBytesSavings[] = "Experimental.Bytes.Network.Savings";
const char kBytesInflation[] = "Experimental.Bytes.Network.Inflation";

}  // namespace internal

DataReductionProxyMetricsObserver::DataReductionProxyMetricsObserver()
    : browser_context_(nullptr),
      num_data_reduction_proxy_resources_(0),
      num_network_resources_(0),
      original_network_bytes_(0),
      network_bytes_proxied_(0),
      network_bytes_(0) {}

DataReductionProxyMetricsObserver::~DataReductionProxyMetricsObserver() {}

// Check if the NavigationData indicates anything about the DataReductionProxy.
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserver::OnCommit(
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
    return STOP_OBSERVING;
  data_reduction_proxy::DataReductionProxyData* data =
      chrome_navigation_data->GetDataReductionProxyData();
  if (!data || !data->used_data_reduction_proxy())
    return STOP_OBSERVING;
  data_ = data->DeepCopy();
  // DataReductionProxy page loads should only occur on HTTP navigations.
  DCHECK(!navigation_handle->GetURL().SchemeIsCryptographic());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!started_in_foreground)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we send a pingback with data collected up to this point.
  if (info.did_commit) {
    RecordPageSizeUMA();
    SendPingback(timing, info);
  }
  return STOP_OBSERVING;
}

void DataReductionProxyMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordPageSizeUMA();
  SendPingback(timing, info);
}

void DataReductionProxyMetricsObserver::RecordPageSizeUMA() const {
  if (!data_)
    return;

  // If the first request didn't complete, don't record UMA.
  if (num_network_resources_ == 0)
    return;

  // TODO(ryansturm): Evaluate if any of the below histograms are unncessary
  // once data is available. crbug.com/682782

  // The percent of resources that went through the data reduction proxy.
  UMA_HISTOGRAM_PERCENTAGE(
      GetConstHistogramWithSuffix(internal::kResourcesPercentProxied),
      (100 * num_data_reduction_proxy_resources_) / num_network_resources_);

  // The percent of bytes that went through the data reduction proxy.
  if (network_bytes_ > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        GetConstHistogramWithSuffix(internal::kBytesPercentProxied),
        static_cast<int>((100 * network_bytes_proxied_) / network_bytes_));
  }

  // If the data reduction proxy caused savings, record the compression ratio;
  // otherwise, record the inflation ratio.
  if (original_network_bytes_ > 0 &&
      original_network_bytes_ >= network_bytes_) {
    UMA_HISTOGRAM_PERCENTAGE(
        GetConstHistogramWithSuffix(internal::kBytesCompressionRatio),
        static_cast<int>((100 * network_bytes_) / original_network_bytes_));
  } else if (original_network_bytes_ > 0) {
    // Inflation should never be above one hundred percent.
    UMA_HISTOGRAM_PERCENTAGE(
        GetConstHistogramWithSuffix(internal::kBytesInflationPercent),
        static_cast<int>((100 * network_bytes_) / original_network_bytes_ -
                         100));
  }

  // Record the number of network resources seen.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kNetworkResources),
      num_network_resources_);

  // Record the number of resources that used data reduction proxy.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kResourcesProxied),
      num_data_reduction_proxy_resources_);

  // Record the number of resources that did not use data reduction proxy.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kResourcesNotProxied),
      num_network_resources_ - num_data_reduction_proxy_resources_);

  // Record the total KB of network bytes.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kNetworkBytes),
                       network_bytes_);

  // Record the total amount of bytes that went through the data reduction
  // proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesProxied),
                       network_bytes_proxied_);

  // Record the total amount of bytes that did not go through the data reduction
  // proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesNotProxied),
                       network_bytes_ - network_bytes_proxied_);

  // Record the total KB of network bytes that the user would have seen without
  // using data reduction proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesOriginal),
                       original_network_bytes_);

  // Record the savings the user saw by using data reduction proxy. If there was
  // inflation instead, record that.
  if (network_bytes_ <= original_network_bytes_) {
    PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesSavings),
                         original_network_bytes_ - network_bytes_);
  } else {
    PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesInflation),
                         network_bytes_proxied_ - original_network_bytes_);
  }
}

void DataReductionProxyMetricsObserver::SendPingback(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // TODO(ryansturm): Move to OnFirstBackgroundEvent to handle some fast
  // shutdown cases. crbug.com/618072
  if (!browser_context_ || !data_)
    return;
  if (data_reduction_proxy::params::IsIncludedInHoldbackFieldTrial() ||
      data_reduction_proxy::params::IsIncludedInTamperDetectionExperiment()) {
    return;
  }
  // Only consider timing events that happened before the first background
  // event.
  base::Optional<base::TimeDelta> response_start;
  base::Optional<base::TimeDelta> load_event_start;
  base::Optional<base::TimeDelta> first_image_paint;
  base::Optional<base::TimeDelta> first_contentful_paint;
  base::Optional<base::TimeDelta> experimental_first_meaningful_paint;
  base::Optional<base::TimeDelta> parse_blocked_on_script_load_duration;
  base::Optional<base::TimeDelta> parse_stop;
  if (WasStartedInForegroundOptionalEventInForeground(timing.response_start,
                                                      info)) {
    response_start = timing.response_start;
  }
  if (WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                      info)) {
    load_event_start = timing.load_event_start;
  }
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_image_paint,
                                                      info)) {
    first_image_paint = timing.first_image_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    first_contentful_paint = timing.first_contentful_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_meaningful_paint, info)) {
    experimental_first_meaningful_paint = timing.first_meaningful_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_blocked_on_script_load_duration, info)) {
    parse_blocked_on_script_load_duration =
        timing.parse_blocked_on_script_load_duration;
  }
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_stop,
                                                      info)) {
    parse_stop = timing.parse_stop;
  }

  DataReductionProxyPageLoadTiming data_reduction_proxy_timing(
      timing.navigation_start, response_start, load_event_start,
      first_image_paint, first_contentful_paint,
      experimental_first_meaningful_paint,
      parse_blocked_on_script_load_duration, parse_stop, network_bytes_,
      original_network_bytes_);
  GetPingbackClient()->SendPingback(*data_, data_reduction_proxy_timing);
}

void DataReductionProxyMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.dom_content_loaded_event_start,
      internal::kHistogramDOMContentLoadedEventFiredSuffix);
}

void DataReductionProxyMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.load_event_start,
      internal::kHistogramLoadEventFiredSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstLayout(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.first_layout, internal::kHistogramFirstLayoutSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(info, data_, timing.first_paint,
                                          internal::kHistogramFirstPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstTextPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.first_text_paint,
      internal::kHistogramFirstTextPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstImagePaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.first_image_paint,
      internal::kHistogramFirstImagePaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.first_contentful_paint,
      internal::kHistogramFirstContentfulPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstMeaningfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.first_meaningful_paint,
      internal::kHistogramFirstMeaningfulPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(info, data_, timing.parse_start,
                                          internal::kHistogramParseStartSuffix);
}

void DataReductionProxyMetricsObserver::OnParseStop(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!WasStartedInForegroundOptionalEventInForeground(timing.parse_stop, info))
    return;

  base::TimeDelta parse_duration =
      timing.parse_stop.value() - timing.parse_start.value();
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, parse_duration,
                               internal::kHistogramParseDurationSuffix);
  RECORD_HISTOGRAMS_FOR_SUFFIX(
      data_, timing.parse_blocked_on_script_load_duration.value(),
      internal::kHistogramParseBlockedOnScriptLoadSuffix);
}

void DataReductionProxyMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestInfo& extra_request_info) {
  if (extra_request_info.was_cached)
    return;
  original_network_bytes_ += extra_request_info.original_network_content_length;
  network_bytes_ += extra_request_info.raw_body_bytes;
  num_network_resources_++;
  if (!extra_request_info.data_reduction_proxy_used)
    return;
  num_data_reduction_proxy_resources_++;
  network_bytes_proxied_ += extra_request_info.raw_body_bytes;
}

DataReductionProxyPingbackClient*
DataReductionProxyMetricsObserver::GetPingbackClient() const {
  return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
             browser_context_)
      ->data_reduction_proxy_service()
      ->pingback_client();
}

}  // namespace data_reduction_proxy
