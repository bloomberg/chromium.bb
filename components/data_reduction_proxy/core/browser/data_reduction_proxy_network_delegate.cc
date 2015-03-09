// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace {

void RecordContentLengthHistograms(
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
    const DataReductionProxyConfigurator* configurator)
    : LayeredNetworkDelegate(network_delegate.Pass()),
      ui_task_runner_(NULL),
      received_content_length_(0),
      original_content_length_(0),
      data_reduction_proxy_enabled_(NULL),
      data_reduction_proxy_config_(config),
      data_reduction_proxy_usage_stats_(NULL),
      data_reduction_proxy_request_options_(request_options),
      configurator_(configurator) {
  DCHECK(data_reduction_proxy_config_);
  DCHECK(data_reduction_proxy_request_options_);
}

DataReductionProxyNetworkDelegate::~DataReductionProxyNetworkDelegate() {
}

void DataReductionProxyNetworkDelegate::InitIODataAndUMA(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DataReductionProxyIOData* io_data,
    BooleanPrefMember* data_reduction_proxy_enabled,
    DataReductionProxyUsageStats* usage_stats) {
  DCHECK(data_reduction_proxy_enabled);
  DCHECK(usage_stats);
  ui_task_runner_ = ui_task_runner;
  data_reduction_proxy_io_data_ = io_data;
  data_reduction_proxy_enabled_ = data_reduction_proxy_enabled;
  data_reduction_proxy_usage_stats_ = usage_stats;
}

base::Value*
DataReductionProxyNetworkDelegate::SessionNetworkStatsInfoToValue() const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("session_received_content_length",
                  base::Int64ToString(received_content_length_));
  dict->SetString("session_original_content_length",
                  base::Int64ToString(original_content_length_));
  return dict;
}

void DataReductionProxyNetworkDelegate::OnResolveProxyInternal(
    const GURL& url,
    int load_flags,
    const net::ProxyService& proxy_service,
    net::ProxyInfo* result) {
  if (configurator_) {
    OnResolveProxyHandler(
        url, load_flags, configurator_->GetProxyConfigOnIOThread(),
        proxy_service.proxy_retry_info(), data_reduction_proxy_config_, result);
  }
}

void DataReductionProxyNetworkDelegate::OnProxyFallbackInternal(
    const net::ProxyServer& bad_proxy,
    int net_error) {
  if (data_reduction_proxy_usage_stats_) {
    data_reduction_proxy_usage_stats_->OnProxyFallback(
        bad_proxy, net_error);
  }
}

void DataReductionProxyNetworkDelegate::OnBeforeSendProxyHeadersInternal(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
  if (data_reduction_proxy_request_options_) {
    data_reduction_proxy_request_options_->MaybeAddRequestHeader(
        request, proxy_info.proxy_server(), headers);
  }
}

void DataReductionProxyNetworkDelegate::OnCompletedInternal(
    net::URLRequest* request,
    bool started) {
  DCHECK(request);
  if (data_reduction_proxy_usage_stats_)
    data_reduction_proxy_usage_stats_->OnUrlRequestCompleted(request, started);

  // Only record for http or https urls.
  bool is_http = request->url().SchemeIs("http");
  bool is_https = request->url().SchemeIs("https");

  if (request->status().status() != net::URLRequestStatus::SUCCESS)
    return;

  // For better accuracy, we use the actual bytes read instead of the length
  // specified with the Content-Length header, which may be inaccurate,
  // or missing, as is the case with chunked encoding.
  int64 received_content_length = request->received_response_content_length();
  if (!request->was_cached() &&          // Don't record cached content
      received_content_length &&         // Zero-byte responses aren't useful.
      (is_http || is_https) &&           // Only record for HTTP or HTTPS urls.
      configurator_) {                   // Used by request type and histograms.
    int64 original_content_length =
        request->response_info().headers->GetInt64HeaderValue(
            "x-original-content-length");
    base::TimeDelta freshness_lifetime =
        request->response_info().headers->GetFreshnessLifetimes(
            request->response_info().response_time).freshness;
    DataReductionProxyRequestType request_type =
        GetDataReductionProxyRequestType(
            *request,
            configurator_->GetProxyConfigOnIOThread(),
            *data_reduction_proxy_config_);

    int64 adjusted_original_content_length =
        GetAdjustedOriginalContentLength(request_type,
                                         original_content_length,
                                         received_content_length);
    AccumulateContentLength(received_content_length,
                            adjusted_original_content_length,
                            request_type);
    RecordContentLengthHistograms(received_content_length,
                                  original_content_length,
                                  freshness_lifetime);

    if (data_reduction_proxy_enabled_ &&
        data_reduction_proxy_usage_stats_) {
      data_reduction_proxy_usage_stats_->RecordBytesHistograms(
          *request,
          *data_reduction_proxy_enabled_,
          configurator_->GetProxyConfigOnIOThread());
    }
    DVLOG(2) << __FUNCTION__
        << " received content length: " << received_content_length
        << " original content length: " << original_content_length
        << " url: " << request->url();
  }
}

void DataReductionProxyNetworkDelegate::AccumulateContentLength(
    int64 received_content_length,
    int64 original_content_length,
    DataReductionProxyRequestType request_type) {
  DCHECK_GE(received_content_length, 0);
  DCHECK_GE(original_content_length, 0);
  if (data_reduction_proxy_enabled_) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DataReductionProxyNetworkDelegate::UpdateContentLengthPrefs,
                   base::Unretained(this),
                   received_content_length,
                   original_content_length,
                   data_reduction_proxy_enabled_->GetValue(),
                   request_type));
  }
  received_content_length_ += received_content_length;
  original_content_length_ += original_content_length;
}

void DataReductionProxyNetworkDelegate::UpdateContentLengthPrefs(
    int received_content_length,
    int original_content_length,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type) {
  if (data_reduction_proxy_io_data_ &&
      data_reduction_proxy_io_data_->service()) {
    DataReductionProxyStatisticsPrefs* data_reduction_proxy_statistics_prefs =
        data_reduction_proxy_io_data_->service()->statistics_prefs();
    int64 total_received = data_reduction_proxy_statistics_prefs->GetInt64(
        data_reduction_proxy::prefs::kHttpReceivedContentLength);
    int64 total_original = data_reduction_proxy_statistics_prefs->GetInt64(
        data_reduction_proxy::prefs::kHttpOriginalContentLength);
    total_received += received_content_length;
    total_original += original_content_length;
    data_reduction_proxy_statistics_prefs->SetInt64(
        data_reduction_proxy::prefs::kHttpReceivedContentLength,
        total_received);
    data_reduction_proxy_statistics_prefs->SetInt64(
        data_reduction_proxy::prefs::kHttpOriginalContentLength,
        total_original);

    UpdateContentLengthPrefsForDataReductionProxy(
        received_content_length,
        original_content_length,
        data_reduction_proxy_enabled,
        request_type,
        base::Time::Now(),
        data_reduction_proxy_statistics_prefs);
  }
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

  if ((load_flags & net::LOAD_BYPASS_DATA_REDUCTION_PROXY) &&
      DataReductionProxyParams::IsIncludedInCriticalPathBypassFieldTrial()) {
    if (!result->is_empty() && !result->is_direct() &&
        config->IsDataReductionProxy(result->proxy_server().host_port_pair(),
                                     NULL)) {
      result->RemoveProxiesWithoutScheme(net::ProxyServer::SCHEME_DIRECT);
    }
  }
}

}  // namespace data_reduction_proxy
