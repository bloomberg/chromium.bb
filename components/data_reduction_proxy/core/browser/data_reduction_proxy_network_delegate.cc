// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h"
#include "net/http/http_response_headers.h"
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
    DataReductionProxyParams* params,
    DataReductionProxyAuthRequestHandler* handler,
    const ProxyConfigGetter& getter)
    : LayeredNetworkDelegate(network_delegate.Pass()),
      ui_task_runner_(NULL),
      received_content_length_(0),
      original_content_length_(0),
      data_reduction_proxy_enabled_(NULL),
      data_reduction_proxy_params_(params),
      data_reduction_proxy_usage_stats_(NULL),
      data_reduction_proxy_auth_request_handler_(handler),
      data_reduction_proxy_statistics_prefs_(NULL),
      proxy_config_getter_(getter) {
  DCHECK(data_reduction_proxy_params_);
  DCHECK(data_reduction_proxy_auth_request_handler_);
}

DataReductionProxyNetworkDelegate::~DataReductionProxyNetworkDelegate() {
}

void DataReductionProxyNetworkDelegate::InitProxyConfigOverrider(
    const OnResolveProxyHandler& proxy_handler) {
  DCHECK(!proxy_config_getter_.is_null());
  on_resolve_proxy_handler_ = proxy_handler;
}

void DataReductionProxyNetworkDelegate::InitStatisticsPrefsAndUMA(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DataReductionProxyStatisticsPrefs* statistics_prefs,
    BooleanPrefMember* data_reduction_proxy_enabled,
    DataReductionProxyUsageStats* usage_stats) {
  DCHECK(statistics_prefs);
  DCHECK(data_reduction_proxy_enabled);
  DCHECK(usage_stats);
  ui_task_runner_ = ui_task_runner;
  data_reduction_proxy_statistics_prefs_ = statistics_prefs;
  data_reduction_proxy_enabled_ = data_reduction_proxy_enabled;
  data_reduction_proxy_usage_stats_ = usage_stats;
}

// static
// TODO(megjablon): Use data_reduction_proxy_delayed_pref_service to read prefs.
// Until updated the pref values may be up to an hour behind on desktop.
base::Value* DataReductionProxyNetworkDelegate::HistoricNetworkStatsInfoToValue(
    PrefService* profile_prefs) {
  int64 total_received = profile_prefs->GetInt64(
      data_reduction_proxy::prefs::kHttpReceivedContentLength);
  int64 total_original = profile_prefs->GetInt64(
      data_reduction_proxy::prefs::kHttpOriginalContentLength);

  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("historic_received_content_length",
                  base::Int64ToString(total_received));
  dict->SetString("historic_original_content_length",
                  base::Int64ToString(total_original));
  return dict;
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
  if (!on_resolve_proxy_handler_.is_null() &&
      !proxy_config_getter_.is_null()) {
    on_resolve_proxy_handler_.Run(
        url, load_flags, proxy_config_getter_.Run(), proxy_service.config(),
        proxy_service.proxy_retry_info(), data_reduction_proxy_params_, result);
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
  if (data_reduction_proxy_auth_request_handler_) {
    data_reduction_proxy_auth_request_handler_->MaybeAddRequestHeader(
        request, proxy_info.proxy_server(), headers);
  }
}

void DataReductionProxyNetworkDelegate::OnCompletedInternal(
    net::URLRequest* request,
    bool started) {
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

  if (!request->was_cached() &&         // Don't record cached content
      received_content_length &&        // Zero-byte responses aren't useful.
      (is_http || is_https)) {          // Only record for HTTP or HTTPS urls.
    int64 original_content_length =
        request->response_info().headers->GetInt64HeaderValue(
            "x-original-content-length");
    base::TimeDelta freshness_lifetime =
        request->response_info().headers->GetFreshnessLifetimes(
            request->response_info().response_time).freshness;
    DataReductionProxyRequestType request_type =
        GetDataReductionProxyRequestType(*request,
                                         *data_reduction_proxy_params_);

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
        data_reduction_proxy_usage_stats_ &&
        !proxy_config_getter_.is_null()) {
      data_reduction_proxy_usage_stats_->RecordBytesHistograms(
          request,
          *data_reduction_proxy_enabled_,
          proxy_config_getter_.Run());
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
  if (data_reduction_proxy_enabled_ &&
      data_reduction_proxy_statistics_prefs_) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&UpdateContentLengthPrefs,
                   received_content_length,
                   original_content_length,
                   data_reduction_proxy_enabled_->GetValue(),
                   request_type,
                   data_reduction_proxy_statistics_prefs_));
  }
  received_content_length_ += received_content_length;
  original_content_length_ += original_content_length;
}

}  // namespace data_reduction_proxy
