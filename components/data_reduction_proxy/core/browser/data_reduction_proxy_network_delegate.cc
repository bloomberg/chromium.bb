// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace {

// |lofi_low_header_added| is set to true iff Lo-Fi "q=low" request header can
// be added to the Chrome proxy headers.
// |received_content_length| is the number of prefilter bytes received.
// |original_content_length| is the length of resource if accessed directly
// without data saver proxy.
// |freshness_lifetime| contains information on how long the resource will be
// fresh for and how long is the usability.
void RecordContentLengthHistograms(bool lofi_low_header_added,
                                   int64 received_content_length,
                                   int64 original_content_length,
                                   const base::TimeDelta& freshness_lifetime) {
  // Add the current resource to these histograms only when a valid
  // X-Original-Content-Length header is present.
  if (original_content_length >= 0) {
    UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthWithValidOCL",
                         received_content_length);
    UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLengthWithValidOCL",
                         original_content_length);
    UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifferenceWithValidOCL",
                         original_content_length - received_content_length);

    // Populate Lo-Fi content length histograms.
    if (lofi_low_header_added) {
      UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthWithValidOCL.LoFiOn",
                           received_content_length);
      UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLengthWithValidOCL.LoFiOn",
                           original_content_length);
      UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifferenceWithValidOCL.LoFiOn",
                           original_content_length - received_content_length);
    }

  } else {
    // Presume the original content length is the same as the received content
    // length if the X-Original-Content-Header is not present.
    original_content_length = received_content_length;
  }
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLength", received_content_length);
  UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLength",
                       original_content_length);
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifference",
                       original_content_length - received_content_length);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpContentFreshnessLifetime",
                              freshness_lifetime.InSeconds(),
                              base::TimeDelta::FromHours(1).InSeconds(),
                              base::TimeDelta::FromDays(30).InSeconds(),
                              100);
  if (freshness_lifetime.InSeconds() <= 0)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable",
                       received_content_length);
  if (freshness_lifetime.InHours() < 4)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable4Hours",
                       received_content_length);

  if (freshness_lifetime.InHours() < 24)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable24Hours",
                       received_content_length);
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyNetworkDelegate::DataReductionProxyNetworkDelegate(
    scoped_ptr<net::NetworkDelegate> network_delegate,
    DataReductionProxyConfig* config,
    DataReductionProxyRequestOptions* request_options,
    const DataReductionProxyConfigurator* configurator,
    DataReductionProxyExperimentsStats* experiments_stats,
    net::NetLog* net_log,
    DataReductionProxyEventCreator* event_creator)
    : LayeredNetworkDelegate(network_delegate.Pass()),
      total_received_bytes_(0),
      total_original_received_bytes_(0),
      data_reduction_proxy_config_(config),
      data_reduction_proxy_bypass_stats_(nullptr),
      data_reduction_proxy_request_options_(request_options),
      data_reduction_proxy_io_data_(nullptr),
      configurator_(configurator),
      experiments_stats_(experiments_stats),
      net_log_(net_log),
      event_creator_(event_creator) {
  DCHECK(data_reduction_proxy_config_);
  DCHECK(data_reduction_proxy_request_options_);
  DCHECK(configurator_);
  DCHECK(experiments_stats_);
  DCHECK(net_log_);
  DCHECK(event_creator_);
}

DataReductionProxyNetworkDelegate::~DataReductionProxyNetworkDelegate() {
}

void DataReductionProxyNetworkDelegate::InitIODataAndUMA(
    DataReductionProxyIOData* io_data,
    DataReductionProxyBypassStats* bypass_stats) {
  DCHECK(bypass_stats);
  data_reduction_proxy_io_data_ = io_data;
  data_reduction_proxy_bypass_stats_ = bypass_stats;
}

base::Value*
DataReductionProxyNetworkDelegate::SessionNetworkStatsInfoToValue() const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("session_received_content_length",
                  base::Int64ToString(total_received_bytes_));
  dict->SetString("session_original_content_length",
                  base::Int64ToString(total_original_received_bytes_));
  return dict;
}

void DataReductionProxyNetworkDelegate::OnResolveProxyInternal(
    const GURL& url,
    int load_flags,
    const net::ProxyService& proxy_service,
    net::ProxyInfo* result) {
  OnResolveProxyHandler(url, load_flags, configurator_->GetProxyConfig(),
                        proxy_service.proxy_retry_info(),
                        data_reduction_proxy_config_, result);
}

void DataReductionProxyNetworkDelegate::OnProxyFallbackInternal(
    const net::ProxyServer& bad_proxy,
    int net_error) {
  if (bad_proxy.is_valid() &&
      data_reduction_proxy_config_->IsDataReductionProxy(
          bad_proxy.host_port_pair(), nullptr)) {
    event_creator_->AddProxyFallbackEvent(net_log_, bad_proxy.ToURI(),
                                          net_error);
  }

  if (data_reduction_proxy_bypass_stats_) {
    data_reduction_proxy_bypass_stats_->OnProxyFallback(
        bad_proxy, net_error);
  }
}

void DataReductionProxyNetworkDelegate::OnBeforeSendProxyHeadersInternal(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
  DCHECK(data_reduction_proxy_config_);

  bool is_using_lofi_mode = false;

  if (data_reduction_proxy_io_data_ &&
      data_reduction_proxy_io_data_->lofi_decider() && request) {
    LoFiDecider* lofi_decider = data_reduction_proxy_io_data_->lofi_decider();
    is_using_lofi_mode = lofi_decider->IsUsingLoFiMode(*request);

    if ((request->load_flags() & net::LOAD_MAIN_FRAME) != 0) {
      // TODO(megjablon): Need to switch to per page.
      data_reduction_proxy_io_data_->SetLoFiModeActiveOnMainFrame(
          is_using_lofi_mode);
    }
  }

  if (data_reduction_proxy_request_options_) {
    data_reduction_proxy_request_options_->MaybeAddRequestHeader(
        request, proxy_info.proxy_server(), headers, is_using_lofi_mode);
  }
}

void DataReductionProxyNetworkDelegate::OnCompletedInternal(
    net::URLRequest* request,
    bool started) {
  DCHECK(request);
  if (data_reduction_proxy_bypass_stats_)
    data_reduction_proxy_bypass_stats_->OnUrlRequestCompleted(request, started);

  // For better accuracy, we use the actual bytes read instead of the length
  // specified with the Content-Length header, which may be inaccurate,
  // or missing, as is the case with chunked encoding.
  int64 received_content_length = request->received_response_content_length();
  if (!request->was_cached() &&   // Don't record cached content
      received_content_length &&  // Zero-byte responses aren't useful.
      request->response_info().network_accessed &&  // Network was accessed.
      request->url().SchemeIsHTTPOrHTTPS()) {
    int64 original_content_length =
        request->response_info().headers->GetInt64HeaderValue(
            "x-original-content-length");
    base::TimeDelta freshness_lifetime =
        request->response_info().headers->GetFreshnessLifetimes(
            request->response_info().response_time).freshness;
    DataReductionProxyRequestType request_type =
        GetDataReductionProxyRequestType(*request,
                                         configurator_->GetProxyConfig(),
                                         *data_reduction_proxy_config_);

    int64 adjusted_original_content_length =
        GetAdjustedOriginalContentLength(request_type,
                                         original_content_length,
                                         received_content_length);
    int64 data_used = request->GetTotalReceivedBytes();
    // TODO(kundaji): Investigate why |compressed_size| can sometimes be
    // less than |received_content_length|. Bug http://crbug/536139.
    if (data_used < received_content_length)
      data_used = received_content_length;

    int64 original_size = data_used;
    if (request_type == VIA_DATA_REDUCTION_PROXY) {
      // TODO(kundaji): Remove headers added by data reduction proxy when
      // computing original size. http://crbug/535701.
      original_size = request->response_info().headers->raw_headers().size() +
                      adjusted_original_content_length;
    }

    std::string mime_type;
    if (request->status().status() == net::URLRequestStatus::SUCCESS)
      request->GetMimeType(&mime_type);

    std::string data_usage_host =
        request->first_party_for_cookies().HostNoBrackets();
    if (data_usage_host.empty()) {
      data_usage_host = request->url().HostNoBrackets();
    }

    AccumulateDataUsage(data_used, original_size, request_type, data_usage_host,
                        mime_type);

    DCHECK(data_reduction_proxy_config_);

    RecordContentLengthHistograms(
        // |data_reduction_proxy_io_data_| can be NULL for Webview.
        data_reduction_proxy_io_data_ &&
            data_reduction_proxy_io_data_->IsEnabled() &&
            data_reduction_proxy_io_data_->lofi_decider() &&
            data_reduction_proxy_io_data_->lofi_decider()->IsUsingLoFiMode(
                *request),
        received_content_length, original_content_length, freshness_lifetime);
    experiments_stats_->RecordBytes(request->request_time(), request_type,
                                    received_content_length,
                                    original_content_length);

    if (data_reduction_proxy_io_data_ && data_reduction_proxy_bypass_stats_) {
      data_reduction_proxy_bypass_stats_->RecordBytesHistograms(
          *request, data_reduction_proxy_io_data_->IsEnabled(),
          configurator_->GetProxyConfig());
    }

    DVLOG(2) << __FUNCTION__
        << " received content length: " << received_content_length
        << " original content length: " << original_content_length
        << " url: " << request->url();
  }
}

void DataReductionProxyNetworkDelegate::AccumulateDataUsage(
    int64 data_used,
    int64 original_size,
    DataReductionProxyRequestType request_type,
    const std::string& data_usage_host,
    const std::string& mime_type) {
  DCHECK_GE(data_used, 0);
  DCHECK_GE(original_size, 0);
  if (data_reduction_proxy_io_data_) {
    data_reduction_proxy_io_data_->UpdateContentLengths(
        data_used, original_size, data_reduction_proxy_io_data_->IsEnabled(),
        request_type, data_usage_host, mime_type);
  }
  total_received_bytes_ += data_used;
  total_original_received_bytes_ += original_size;
}

void OnResolveProxyHandler(const GURL& url,
                           int load_flags,
                           const net::ProxyConfig& data_reduction_proxy_config,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           const DataReductionProxyConfig* config,
                           net::ProxyInfo* result) {
  DCHECK(config);
  DCHECK(result->is_empty() || result->is_direct() ||
         !config->IsDataReductionProxy(result->proxy_server().host_port_pair(),
                                       NULL));
  if (data_reduction_proxy_config.is_valid() &&
      result->proxy_server().is_direct() &&
      result->proxy_list().size() == 1 &&
      !url.SchemeIsWSOrWSS()) {
    net::ProxyInfo data_reduction_proxy_info;
    data_reduction_proxy_config.proxy_rules().Apply(
        url, &data_reduction_proxy_info);
    data_reduction_proxy_info.DeprioritizeBadProxies(proxy_retry_info);
    if (!data_reduction_proxy_info.proxy_server().is_direct())
      result->OverrideProxyList(data_reduction_proxy_info.proxy_list());
  }
}

}  // namespace data_reduction_proxy
