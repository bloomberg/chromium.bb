// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"

#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_list.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

// Adds non-empty entries in |data_reduction_proxies| to the retry map
// maintained by the proxy service of the request. Adds
// |data_reduction_proxies.second| to the retry list only if |bypass_all| is
// true.
void MarkProxiesAsBadUntil(
    net::URLRequest* request,
    const base::TimeDelta& bypass_duration,
    bool bypass_all,
    const std::vector<net::ProxyServer>& data_reduction_proxies) {
  DCHECK(!data_reduction_proxies.empty());
  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  net::ProxyList proxy_list;
  std::vector<net::ProxyServer> additional_bad_proxies;
  for (const net::ProxyServer& proxy_server : data_reduction_proxies) {
    DCHECK(proxy_server.is_valid());
    proxy_list.AddProxyServer(proxy_server);
    if (!bypass_all)
      break;
    additional_bad_proxies.push_back(proxy_server);
  }
  proxy_list.AddProxyServer(net::ProxyServer::Direct());

  net::ProxyInfo proxy_info;
  proxy_info.UseProxyList(proxy_list);
  DCHECK(request->context());
  net::ProxyService* proxy_service = request->context()->proxy_service();
  DCHECK(proxy_service);

  proxy_service->MarkProxiesAsBadUntil(
      proxy_info, bypass_duration, additional_bad_proxies, request->net_log());
}

void ReportResponseProxyServerStatusHistogram(
    DataReductionProxyBypassProtocol::ResponseProxyServerStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.ResponseProxyServerStatus", status,
      DataReductionProxyBypassProtocol::RESPONSE_PROXY_SERVER_STATUS_MAX);
}

}  // namespace

DataReductionProxyBypassProtocol::DataReductionProxyBypassProtocol(
    DataReductionProxyConfig* config)
    : config_(config) {
  DCHECK(config_);
}

DataReductionProxyBypassProtocol::~DataReductionProxyBypassProtocol() {
}

bool DataReductionProxyBypassProtocol::MaybeBypassProxyAndPrepareToRetry(
    net::URLRequest* request,
    DataReductionProxyBypassType* proxy_bypass_type,
    DataReductionProxyInfo* data_reduction_proxy_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request);

  DataReductionProxyTypeInfo data_reduction_proxy_type_info;
  DataReductionProxyBypassType bypass_type;

  const net::HttpResponseHeaders* response_headers =
      request->response_info().headers.get();
  bool retry;
  if (!response_headers) {
    retry = HandleInValidResponseHeadersCase(
        *request, data_reduction_proxy_info, &data_reduction_proxy_type_info,
        &bypass_type);

  } else {
    retry = HandleValidResponseHeadersCase(
        *request, proxy_bypass_type, data_reduction_proxy_info,
        &data_reduction_proxy_type_info, &bypass_type);
  }
  if (!retry)
    return false;

  if (data_reduction_proxy_info->mark_proxies_as_bad) {
    MarkProxiesAsBadUntil(request, data_reduction_proxy_info->bypass_duration,
                          data_reduction_proxy_info->bypass_all,
                          data_reduction_proxy_type_info.proxy_servers);
  } else {
    request->SetLoadFlags(request->load_flags() | net::LOAD_BYPASS_CACHE |
                          net::LOAD_BYPASS_PROXY);
  }

  return bypass_type == BYPASS_EVENT_TYPE_CURRENT || !response_headers ||
         net::HttpUtil::IsMethodIdempotent(request->method());
}

bool DataReductionProxyBypassProtocol::HandleInValidResponseHeadersCase(
    const net::URLRequest& request,
    DataReductionProxyInfo* data_reduction_proxy_info,
    DataReductionProxyTypeInfo* data_reduction_proxy_type_info,
    DataReductionProxyBypassType* bypass_type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(nullptr, request.response_info().headers.get());

  // Handling of invalid response headers is enabled by default.
  if (!GetFieldTrialParamByFeatureAsBool(
          features::kDataReductionProxyRobustConnection,
          "handle_invalid_respnse_headers", true)) {
    return false;
  }

  if (!config_->WasDataReductionProxyUsed(&request,
                                          data_reduction_proxy_type_info)) {
    return false;
  }

  DCHECK(request.url().SchemeIs(url::kHttpScheme));

  net::URLRequestStatus status = request.status();
  DCHECK(!status.is_success());
  DCHECK_NE(net::OK, status.error());
  if (status.error() == net::ERR_IO_PENDING ||
      status.error() == net::ERR_NETWORK_CHANGED ||
      status.error() == net::ERR_INTERNET_DISCONNECTED ||
      status.error() == net::ERR_ABORTED || std::abs(status.error()) >= 400) {
    // No need to retry the request or mark the proxy as bad. Only bypass on
    // System related errors, connection related errors and certificate errors.
    return false;
  }

  static_assert(
      net::ERR_CONNECTION_RESET > -400 && net::ERR_SSL_PROTOCOL_ERROR > -400,
      "net error is not handled");

  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "DataReductionProxy.InvalidResponseHeadersReceived.NetError",
      -status.error());

  data_reduction_proxy_info->bypass_all = false;
  data_reduction_proxy_info->mark_proxies_as_bad = true;
  data_reduction_proxy_info->bypass_duration = base::TimeDelta::FromMinutes(5);
  data_reduction_proxy_info->bypass_action = BYPASS_ACTION_TYPE_BYPASS;
  *bypass_type = BYPASS_EVENT_TYPE_MEDIUM;

  return true;
}

bool DataReductionProxyBypassProtocol::HandleValidResponseHeadersCase(
    const net::URLRequest& request,
    DataReductionProxyBypassType* proxy_bypass_type,
    DataReductionProxyInfo* data_reduction_proxy_info,
    DataReductionProxyTypeInfo* data_reduction_proxy_type_info,
    DataReductionProxyBypassType* bypass_type) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Empty implies either that the request was served from cache or that
  // request was served directly from the origin.
  const net::HttpResponseHeaders* response_headers =
      request.response_info().headers.get();

  DCHECK(response_headers);
  if (!request.proxy_server().is_valid() ||
      request.proxy_server().is_direct() ||
      request.proxy_server().host_port_pair().IsEmpty()) {
    ReportResponseProxyServerStatusHistogram(
        RESPONSE_PROXY_SERVER_STATUS_EMPTY);
    return false;
  }

  if (!config_->WasDataReductionProxyUsed(&request,
                                          data_reduction_proxy_type_info)) {
    if (!HasDataReductionProxyViaHeader(*response_headers, nullptr)) {
      ReportResponseProxyServerStatusHistogram(
          RESPONSE_PROXY_SERVER_STATUS_NON_DRP_NO_VIA);
      return false;
    }
    ReportResponseProxyServerStatusHistogram(
        RESPONSE_PROXY_SERVER_STATUS_NON_DRP_WITH_VIA);

    // If the |proxy_server| doesn't match any of the currently configured
    // Data Reduction Proxies, but it still has the Data Reduction Proxy via
    // header, then apply the bypass logic regardless.
    // TODO(sclittle): Remove this workaround once http://crbug.com/476610 is
    // fixed.
    const net::HostPortPair host_port_pair =
        !request.proxy_server().is_valid() || request.proxy_server().is_direct()
            ? net::HostPortPair()
            : request.proxy_server().host_port_pair();
    data_reduction_proxy_type_info->proxy_servers.push_back(
        net::ProxyServer(net::ProxyServer::SCHEME_HTTPS, host_port_pair));
    data_reduction_proxy_type_info->proxy_servers.push_back(
        net::ProxyServer(net::ProxyServer::SCHEME_HTTP, host_port_pair));
    data_reduction_proxy_type_info->proxy_index = 0;
  } else {
    ReportResponseProxyServerStatusHistogram(RESPONSE_PROXY_SERVER_STATUS_DRP);
  }
  if (data_reduction_proxy_type_info->proxy_servers.empty())
    return false;

  // At this point, the response is expected to have the data reduction proxy
  // via header, so detect and report cases where the via header is missing.
  DataReductionProxyBypassStats::DetectAndRecordMissingViaHeaderResponseCode(
      data_reduction_proxy_type_info->proxy_index == 0, *response_headers);

  // GetDataReductionProxyBypassType will only log a net_log event if a bypass
  // command was sent via the data reduction proxy headers
  *bypass_type = GetDataReductionProxyBypassType(
      request.url_chain(), *response_headers, data_reduction_proxy_info);

  if (proxy_bypass_type)
    *proxy_bypass_type = *bypass_type;
  if (*bypass_type == BYPASS_EVENT_TYPE_MAX)
    return false;

  DCHECK(request.context());
  DCHECK(request.context()->proxy_service());
  net::ProxyServer proxy_server =
      data_reduction_proxy_type_info->proxy_servers.front();

  // Only record UMA if the proxy isn't already on the retry list.
  if (!config_->IsProxyBypassed(
          request.context()->proxy_service()->proxy_retry_info(), proxy_server,
          NULL)) {
    DataReductionProxyBypassStats::RecordDataReductionProxyBypassInfo(
        data_reduction_proxy_type_info->proxy_index == 0,
        data_reduction_proxy_info->bypass_all, proxy_server, *bypass_type);
  }
  return true;
}

}  // namespace data_reduction_proxy
