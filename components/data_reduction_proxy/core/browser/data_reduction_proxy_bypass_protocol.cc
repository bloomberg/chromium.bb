// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_list.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace {

// Adds non-empty entries in |data_reduction_proxies| to the retry map
// maintained by the proxy service of the request. Adds
// |data_reduction_proxies.second| to the retry list only if |bypass_all| is
// true.
void MarkProxiesAsBadUntil(
    net::URLRequest* request,
    const base::TimeDelta& bypass_duration,
    bool bypass_all,
    const std::pair<net::ProxyServer, net::ProxyServer>&
      data_reduction_proxies) {
  DCHECK(data_reduction_proxies.first.is_valid());
  DCHECK(!data_reduction_proxies.first.host_port_pair().IsEmpty());
  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  net::ProxyList proxy_list;
  net::ProxyServer primary = data_reduction_proxies.first;
  if (primary.is_valid())
    proxy_list.AddProxyServer(primary);
  net::ProxyServer fallback;
  if (bypass_all) {
    if (data_reduction_proxies.second.is_valid() &&
        !data_reduction_proxies.second.host_port_pair().IsEmpty())
      fallback = data_reduction_proxies.second;
    if (fallback.is_valid())
      proxy_list.AddProxyServer(fallback);
    proxy_list.AddProxyServer(net::ProxyServer::Direct());
  }
  net::ProxyInfo proxy_info;
  proxy_info.UseProxyList(proxy_list);
  DCHECK(request->context());
  net::ProxyService* proxy_service = request->context()->proxy_service();
  DCHECK(proxy_service);

  proxy_service->MarkProxiesAsBadUntil(proxy_info,
                                       bypass_duration,
                                       fallback,
                                       request->net_log());
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyBypassProtocol::DataReductionProxyBypassProtocol(
    DataReductionProxyConfig* config)
    : config_(config) {
  DCHECK(config_);
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
}

DataReductionProxyBypassProtocol::~DataReductionProxyBypassProtocol() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

bool DataReductionProxyBypassProtocol::MaybeBypassProxyAndPrepareToRetry(
    net::URLRequest* request,
    DataReductionProxyBypassType* proxy_bypass_type,
    DataReductionProxyInfo* data_reduction_proxy_info) {
  DCHECK(request);
  const net::HttpResponseHeaders* response_headers =
      request->response_info().headers.get();
  if (!response_headers)
    return false;

  // Empty implies either that the request was served from cache or that
  // request was served directly from the origin.
  // TODO(sclittle): Add UMA to confirm that the |proxy_server| is never empty
  // when the response has the Data Reduction Proxy via header.
  if (request->proxy_server().IsEmpty())
    return false;

  DataReductionProxyTypeInfo data_reduction_proxy_type_info;
  if (!config_->WasDataReductionProxyUsed(request,
                                          &data_reduction_proxy_type_info)) {
    if (!HasDataReductionProxyViaHeader(response_headers, nullptr))
      return false;

    // If the |proxy_server| doesn't match any of the currently configured Data
    // Reduction Proxies, but it still has the Data Reduction Proxy via header,
    // then apply the bypass logic regardless.
    // TODO(sclittle): Add UMA to record how often this occurs, and remove this
    // workaround once http://crbug.com/476610 is fixed.
    data_reduction_proxy_type_info.proxy_servers.first = net::ProxyServer(
        net::ProxyServer::SCHEME_HTTPS, request->proxy_server());
    data_reduction_proxy_type_info.proxy_servers.second = net::ProxyServer(
        net::ProxyServer::SCHEME_HTTP, request->proxy_server());
    data_reduction_proxy_type_info.is_alternative = false;
    data_reduction_proxy_type_info.is_fallback = false;
    data_reduction_proxy_type_info.is_ssl = request->url().SchemeIsSecure();
  }
  // TODO(bengr): Implement bypass for CONNECT tunnel.
  if (data_reduction_proxy_type_info.is_ssl)
    return false;

  const net::ProxyServer& first =
      data_reduction_proxy_type_info.proxy_servers.first;
  if (!first.is_valid() || first.host_port_pair().IsEmpty())
    return false;

  // At this point, the response is expected to have the data reduction proxy
  // via header, so detect and report cases where the via header is missing.
  const net::ProxyServer& second =
      data_reduction_proxy_type_info.proxy_servers.second;
  DataReductionProxyBypassStats::DetectAndRecordMissingViaHeaderResponseCode(
      second.is_valid() && !second.host_port_pair().IsEmpty(),
      response_headers);

  if (DataReductionProxyParams::
          IsIncludedInRelaxMissingViaHeaderOtherBypassFieldTrial() &&
      HasDataReductionProxyViaHeader(response_headers, NULL)) {
    DCHECK(config_->IsDataReductionProxy(request->proxy_server(), NULL));
    via_header_producing_proxies_.insert(request->proxy_server());
  }

  // GetDataReductionProxyBypassType will only log a net_log event if a bypass
  // command was sent via the data reduction proxy headers
  DataReductionProxyBypassType bypass_type = GetDataReductionProxyBypassType(
      response_headers, data_reduction_proxy_info);

  if (bypass_type == BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER) {
    if (DataReductionProxyParams::
            IsIncludedInRemoveMissingViaHeaderOtherBypassFieldTrial() ||
        (DataReductionProxyParams::
             IsIncludedInRelaxMissingViaHeaderOtherBypassFieldTrial() &&
         via_header_producing_proxies_.find(request->proxy_server()) !=
             via_header_producing_proxies_.end())) {
      // Ignore MISSING_VIA_HEADER_OTHER proxy bypass events if the client is
      // part of the field trial to remove these kinds of bypasses, or if the
      // client is part of the field trial to relax this bypass rule and Chrome
      // has previously seen a data reduction proxy via header on a response
      // through this proxy since the last network change.
      bypass_type = BYPASS_EVENT_TYPE_MAX;
    }
  }

  if (proxy_bypass_type)
    *proxy_bypass_type = bypass_type;
  if (bypass_type == BYPASS_EVENT_TYPE_MAX)
    return false;

  DCHECK(request->context());
  DCHECK(request->context()->proxy_service());
  net::ProxyServer proxy_server =
      data_reduction_proxy_type_info.proxy_servers.first;

  // Only record UMA if the proxy isn't already on the retry list.
  if (!config_->IsProxyBypassed(
          request->context()->proxy_service()->proxy_retry_info(), proxy_server,
          NULL)) {
    DataReductionProxyBypassStats::RecordDataReductionProxyBypassInfo(
        second.is_valid() && !second.host_port_pair().IsEmpty(),
        data_reduction_proxy_info->bypass_all, proxy_server, bypass_type);
  }

  if (data_reduction_proxy_info->mark_proxies_as_bad) {
    MarkProxiesAsBadUntil(request, data_reduction_proxy_info->bypass_duration,
                          data_reduction_proxy_info->bypass_all,
                          data_reduction_proxy_type_info.proxy_servers);
  } else {
    request->SetLoadFlags(request->load_flags() |
                          net::LOAD_DISABLE_CACHE |
                          net::LOAD_BYPASS_PROXY);
  }

  // Retry if block-once was specified or if method is idempotent.
  return  bypass_type == BYPASS_EVENT_TYPE_CURRENT ||
      IsRequestIdempotent(request);
}

// static
bool DataReductionProxyBypassProtocol::IsRequestIdempotent(
    const net::URLRequest* request) {
  DCHECK(request);
  if (request->method() == "GET" ||
      request->method() == "OPTIONS" ||
      request->method() == "HEAD" ||
      request->method() == "PUT" ||
      request->method() == "DELETE" ||
      request->method() == "TRACE")
    return true;
  return false;
}

void DataReductionProxyBypassProtocol::OnIPAddressChanged() {
  via_header_producing_proxies_.clear();
}

}  // namespace data_reduction_proxy
