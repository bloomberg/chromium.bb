// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"

#include "base/metrics/histogram.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using base::MessageLoopProxy;
using net::HostPortPair;
using net::ProxyServer;
using net::ProxyService;
using net::NetworkChangeNotifier;
using net::URLRequest;

namespace data_reduction_proxy {

DataReductionProxyUsageStats::DataReductionProxyUsageStats(
    DataReductionProxyParams* params,
    MessageLoopProxy* ui_thread_proxy,
    MessageLoopProxy* io_thread_proxy)
    : data_reduction_proxy_params_(params),
      last_bypass_type_(ProxyService::BYPASS_EVENT_TYPE_MAX),
      triggering_request_(true),
      ui_thread_proxy_(ui_thread_proxy),
      io_thread_proxy_(io_thread_proxy),
      eligible_num_requests_through_proxy_(0),
      actual_num_requests_through_proxy_(0) {
  NetworkChangeNotifier::AddNetworkChangeObserver(this);
};

DataReductionProxyUsageStats::~DataReductionProxyUsageStats() {
  NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
};

void DataReductionProxyUsageStats::OnUrlRequestCompleted(
    const net::URLRequest* request, bool started) {
  DCHECK(io_thread_proxy_->BelongsToCurrentThread());

  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
    if (data_reduction_proxy_params_->IsDataReductionProxyEligible(request)) {
      bool was_received_via_proxy =
          data_reduction_proxy_params_->WasDataReductionProxyUsed(
              request, NULL);
      ui_thread_proxy_->PostTask(FROM_HERE, base::Bind(
          &DataReductionProxyUsageStats::IncRequestCountsOnUiThread,
          base::Unretained(this), was_received_via_proxy));
    }
  }
}

/**
 * Determines if the data reduction proxy is currently unreachable. We keep
 * track of count of requests which go through data reduction proxy as well as
 * count of requests which are eligible to go through the proxy. If and only if
 * no requests go through proxy and some requests were eligible, we conclude
 * that the proxy is unreachable.
 *
 * Returns true if the data reduction proxy is unreachable.
 */
bool DataReductionProxyUsageStats::isDataReductionProxyUnreachable() {
  DCHECK(ui_thread_proxy_->BelongsToCurrentThread());

  return eligible_num_requests_through_proxy_ > 0 &&
      actual_num_requests_through_proxy_ == 0;
}

void DataReductionProxyUsageStats::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ui_thread_proxy_->PostTask(FROM_HERE, base::Bind(
      &DataReductionProxyUsageStats::ClearRequestCountsOnUiThread,
      base::Unretained(this)));
}

void DataReductionProxyUsageStats::IncRequestCountsOnUiThread(
    bool was_received_via_proxy) {
  DCHECK(ui_thread_proxy_->BelongsToCurrentThread());
  if (was_received_via_proxy) {
    actual_num_requests_through_proxy_++;
  }
  eligible_num_requests_through_proxy_++;

  // To account for the case when the proxy works for a little while and then
  // gets blocked, we reset the counts occasionally.
  if (eligible_num_requests_through_proxy_ > 50
      && actual_num_requests_through_proxy_ > 0) {
    ClearRequestCountsOnUiThread();
  }
}

void DataReductionProxyUsageStats::ClearRequestCountsOnUiThread() {
  DCHECK(ui_thread_proxy_->BelongsToCurrentThread());
  eligible_num_requests_through_proxy_ = 0;
  actual_num_requests_through_proxy_ = 0;
}

void DataReductionProxyUsageStats::SetBypassType(
    ProxyService::DataReductionProxyBypassType type) {
  last_bypass_type_ = type;
  triggering_request_ = true;
}

void DataReductionProxyUsageStats::RecordBypassedBytesHistograms(
    net::URLRequest& request,
    const BooleanPrefMember& data_reduction_proxy_enabled) {
  int64 content_length = request.received_response_content_length();
  if (data_reduction_proxy_params_->WasDataReductionProxyUsed(&request, NULL)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyUsageStats::NOT_BYPASSED,
                        content_length);
    return;
  }

  if (data_reduction_proxy_enabled.GetValue() &&
      request.url().SchemeIs(url::kHttpsScheme)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyUsageStats::SSL,
                        content_length);
    return;
  }

  if (data_reduction_proxy_enabled.GetValue() &&
      !data_reduction_proxy_params_->IsDataReductionProxyEligible(&request)) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyUsageStats::LOCAL_BYPASS_RULES,
                        content_length);
    return;
  }

  if (triggering_request_) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyUsageStats::TRIGGERING_REQUEST,
                        content_length);
    triggering_request_ = false;
  }

  std::string mime_type;
  request.GetMimeType(&mime_type);
  // MIME types are named by <media-type>/<subtype>. We check to see if the
  // media type is audio or video.
  if (mime_type.compare(0, 6, "audio/") == 0  ||
      mime_type.compare(0, 6, "video/") == 0) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyUsageStats::AUDIO_VIDEO,
                        content_length);
  }

  if (last_bypass_type_ != ProxyService::BYPASS_EVENT_TYPE_MAX) {
    RecordBypassedBytes(last_bypass_type_,
                        DataReductionProxyUsageStats::BYPASSED_BYTES_TYPE_MAX,
                        content_length);
    return;
  }

  if (data_reduction_proxy_params_->
          AreDataReductionProxiesBypassed(request, NULL)) {
      RecordBypassedBytes(last_bypass_type_,
                          DataReductionProxyUsageStats::NETWORK_ERROR,
                          content_length);
  }
}

void DataReductionProxyUsageStats::RecordBypassedBytes(
    ProxyService::DataReductionProxyBypassType bypass_type,
    DataReductionProxyUsageStats::BypassedBytesType bypassed_bytes_type,
    int64 content_length) {
  // Individual histograms are needed to count the bypassed bytes for each
  // bypass type so that we can see the size of requests. This helps us
  // remove outliers that would skew the sum of bypassed bytes for each type.
  switch (bypassed_bytes_type) {
    case DataReductionProxyUsageStats::NOT_BYPASSED:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.NotBypassed", content_length);
      break;
    case DataReductionProxyUsageStats::SSL:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.SSL", content_length);
      break;
    case DataReductionProxyUsageStats::LOCAL_BYPASS_RULES:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.LocalBypassRules",
          content_length);
      break;
    case DataReductionProxyUsageStats::AUDIO_VIDEO:
      if (last_bypass_type_ == ProxyService::SHORT_BYPASS) {
        UMA_HISTOGRAM_COUNTS(
            "DataReductionProxy.BypassedBytes.ShortAudioVideo",
            content_length);
      }
      break;
    case DataReductionProxyUsageStats::TRIGGERING_REQUEST:
      switch (bypass_type) {
        case ProxyService::SHORT_BYPASS:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
              content_length);
          break;
        case ProxyService::MEDIUM_BYPASS:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
              content_length);
          break;
        case ProxyService::LONG_BYPASS:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
              content_length);
          break;
        default:
          break;
      }
      break;
    case DataReductionProxyUsageStats::NETWORK_ERROR:
      UMA_HISTOGRAM_COUNTS(
          "DataReductionProxy.BypassedBytes.NetworkErrorOther",
          content_length);
      break;
    case DataReductionProxyUsageStats::BYPASSED_BYTES_TYPE_MAX:
      switch (bypass_type) {
        case ProxyService::CURRENT_BYPASS:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.Current",
                               content_length);
          break;
        case ProxyService::SHORT_BYPASS:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.ShortAll",
                               content_length);
          break;
        case ProxyService::MEDIUM_BYPASS:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.MediumAll",
                               content_length);
          break;
        case ProxyService::LONG_BYPASS:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.LongAll",
                               content_length);
          break;
        case ProxyService::MISSING_VIA_HEADER_4XX:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MissingViaHeader4xx",
              content_length);
          break;
        case ProxyService::MISSING_VIA_HEADER_OTHER:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
              content_length);
          break;
        case ProxyService::MALFORMED_407:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.Malformed407",
                               content_length);
         break;
        case ProxyService::STATUS_500_HTTP_INTERNAL_SERVER_ERROR:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes."
              "Status500HttpInternalServerError",
              content_length);
          break;
        case ProxyService::STATUS_502_HTTP_BAD_GATEWAY:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
              content_length);
          break;
        case ProxyService::STATUS_503_HTTP_SERVICE_UNAVAILABLE:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes."
              "Status503HttpServiceUnavailable",
              content_length);
          break;
        default:
          break;
      }
      break;
  }
}

}  // namespace data_reduction_proxy


