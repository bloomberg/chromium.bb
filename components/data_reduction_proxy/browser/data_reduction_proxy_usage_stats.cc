// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
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

namespace {

// Records a net error code that resulted in bypassing the data reduction
// proxy (|is_primary| is true) or the data reduction proxy fallback.
void RecordDataReductionProxyBypassOnNetworkError(
    bool is_primary,
    const ProxyServer& proxy_server,
    int net_error) {
  if (is_primary) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "DataReductionProxy.BypassOnNetworkErrorPrimary",
        std::abs(net_error));
    return;
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "DataReductionProxy.BypassOnNetworkErrorFallback",
      std::abs(net_error));
}

}  // namespace

// static
void DataReductionProxyUsageStats::RecordDataReductionProxyBypassInfo(
    bool is_primary,
    bool bypass_all,
    const net::ProxyServer& proxy_server,
    DataReductionProxyBypassType bypass_type) {
  if (bypass_all) {
    if (is_primary) {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BlockTypePrimary",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BlockTypeFallback",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    }
  } else {
    if (is_primary) {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassTypePrimary",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.BypassTypeFallback",
                                bypass_type, BYPASS_EVENT_TYPE_MAX);
    }
  }
}

DataReductionProxyUsageStats::DataReductionProxyUsageStats(
    DataReductionProxyParams* params,
    MessageLoopProxy* ui_thread_proxy)
    : data_reduction_proxy_params_(params),
      last_bypass_type_(BYPASS_EVENT_TYPE_MAX),
      triggering_request_(true),
      ui_thread_proxy_(ui_thread_proxy),
      eligible_num_requests_through_proxy_(0),
      actual_num_requests_through_proxy_(0),
      unavailable_(false) {
  NetworkChangeNotifier::AddNetworkChangeObserver(this);
};

DataReductionProxyUsageStats::~DataReductionProxyUsageStats() {
  NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
};

void DataReductionProxyUsageStats::OnUrlRequestCompleted(
    const net::URLRequest* request, bool started) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
    if (data_reduction_proxy_params_->IsDataReductionProxyEligible(request)) {
      bool was_received_via_proxy =
          data_reduction_proxy_params_->WasDataReductionProxyUsed(
              request, NULL);
      IncrementRequestCounts(was_received_via_proxy);
    }
  }
}

void DataReductionProxyUsageStats::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ClearRequestCounts();
}

void DataReductionProxyUsageStats::IncrementRequestCounts(
    bool was_received_via_proxy) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (was_received_via_proxy) {
    actual_num_requests_through_proxy_++;
  }
  eligible_num_requests_through_proxy_++;

  // To account for the case when the proxy works for a little while and then
  // gets blocked, we reset the counts occasionally.
  if (eligible_num_requests_through_proxy_ > 50
      && actual_num_requests_through_proxy_ > 0) {
    ClearRequestCounts();
  } else {
    MaybeNotifyUnavailability();
  }
}

void DataReductionProxyUsageStats::ClearRequestCounts() {
  DCHECK(thread_checker_.CalledOnValidThread());
  eligible_num_requests_through_proxy_ = 0;
  actual_num_requests_through_proxy_ = 0;
  MaybeNotifyUnavailability();
}

void DataReductionProxyUsageStats::MaybeNotifyUnavailability() {
  bool prev_unavailable = unavailable_;
  unavailable_ = (eligible_num_requests_through_proxy_ > 0 &&
      actual_num_requests_through_proxy_ == 0);
  if (prev_unavailable != unavailable_) {
     ui_thread_proxy_->PostTask(FROM_HERE, base::Bind(
          &DataReductionProxyUsageStats::NotifyUnavailabilityOnUIThread,
          base::Unretained(this),
          unavailable_));
  }
}

void DataReductionProxyUsageStats::NotifyUnavailabilityOnUIThread(
    bool unavailable) {
  DCHECK(ui_thread_proxy_->BelongsToCurrentThread());
  if (!unavailable_callback_.is_null())
    unavailable_callback_.Run(unavailable);
}

void DataReductionProxyUsageStats::SetBypassType(
    DataReductionProxyBypassType type) {
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

    // We only record when audio or video triggers a bypass. We don't care
    // about audio and video bypassed as collateral damage.
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
  }

  if (last_bypass_type_ != BYPASS_EVENT_TYPE_MAX) {
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

void DataReductionProxyUsageStats::RecordBypassEventHistograms(
    const net::ProxyServer& bypassed_proxy,
    int net_error,
    bool did_fallback) const {
  DataReductionProxyTypeInfo data_reduction_proxy_info;
  if (bypassed_proxy.is_valid() && !bypassed_proxy.is_direct() &&
      data_reduction_proxy_params_->IsDataReductionProxy(
      bypassed_proxy.host_port_pair(), &data_reduction_proxy_info)) {
    if (data_reduction_proxy_info.is_ssl)
      return;
    if (!data_reduction_proxy_info.is_fallback) {
      RecordDataReductionProxyBypassInfo(
          true, false, bypassed_proxy, BYPASS_EVENT_TYPE_NETWORK_ERROR);
      RecordDataReductionProxyBypassOnNetworkError(
          true, bypassed_proxy, net_error);
    } else {
      RecordDataReductionProxyBypassInfo(
          false, false, bypassed_proxy, BYPASS_EVENT_TYPE_NETWORK_ERROR);
      RecordDataReductionProxyBypassOnNetworkError(
          false, bypassed_proxy, net_error);
    }
  }
}

void DataReductionProxyUsageStats::RecordBypassedBytes(
    DataReductionProxyBypassType bypass_type,
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
      if (last_bypass_type_ == BYPASS_EVENT_TYPE_SHORT) {
        UMA_HISTOGRAM_COUNTS(
            "DataReductionProxy.BypassedBytes.ShortAudioVideo",
            content_length);
      }
      break;
    case DataReductionProxyUsageStats::TRIGGERING_REQUEST:
      switch (bypass_type) {
        case BYPASS_EVENT_TYPE_SHORT:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_MEDIUM:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_LONG:
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
        case BYPASS_EVENT_TYPE_CURRENT:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.Current",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_SHORT:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.ShortAll",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_MEDIUM:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.MediumAll",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_LONG:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.LongAll",
                               content_length);
          break;
        case BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MissingViaHeader4xx",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_MALFORMED_407:
          UMA_HISTOGRAM_COUNTS("DataReductionProxy.BypassedBytes.Malformed407",
                               content_length);
         break;
        case BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes."
              "Status500HttpInternalServerError",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY:
          UMA_HISTOGRAM_COUNTS(
              "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
              content_length);
          break;
        case BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE:
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


