// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_protocol.h"

#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
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

bool SetProxyServerFromGURL(const GURL& gurl,
                            net::ProxyServer* proxy_server) {
  DCHECK(proxy_server);
  if (!gurl.SchemeIsHTTPOrHTTPS())
    return false;
  *proxy_server = net::ProxyServer(gurl.SchemeIs(url::kHttpScheme) ?
                                       net::ProxyServer::SCHEME_HTTP :
                                       net::ProxyServer::SCHEME_HTTPS,
                                   net::HostPortPair::FromURL(gurl));
  return true;
}

}  // namespace

namespace data_reduction_proxy {

bool MaybeBypassProxyAndPrepareToRetry(
    const DataReductionProxyParams* data_reduction_proxy_params,
    net::URLRequest* request,
    DataReductionProxyBypassType* proxy_bypass_type,
    DataReductionProxyEventStore* event_store) {
  DCHECK(request);
  if (!data_reduction_proxy_params)
    return false;

  const net::HttpResponseHeaders* response_headers =
      request->response_info().headers.get();
  if (!response_headers)
    return false;

  // Empty implies either that the request was served from cache or that
  // request was served directly from the origin.
  if (request->proxy_server().IsEmpty())
    return false;

  DataReductionProxyTypeInfo data_reduction_proxy_type_info;
  if (!data_reduction_proxy_params->WasDataReductionProxyUsed(
          request, &data_reduction_proxy_type_info)) {
    return false;
  }
  // TODO(bengr): Implement bypass for CONNECT tunnel.
  if (data_reduction_proxy_type_info.is_ssl)
    return false;

  if (data_reduction_proxy_type_info.proxy_servers.first.is_empty())
    return false;

  // At this point, the response is expected to have the data reduction proxy
  // via header, so detect and report cases where the via header is missing.
  DataReductionProxyUsageStats::DetectAndRecordMissingViaHeaderResponseCode(
      !data_reduction_proxy_type_info.proxy_servers.second.is_empty(),
      response_headers);

  // GetDataReductionProxyBypassType will only log a net_log event if a bypass
  // command was sent via the data reduction proxy headers
  bool event_logged = false;
  DataReductionProxyInfo data_reduction_proxy_info;
  DataReductionProxyBypassType bypass_type =
      GetDataReductionProxyBypassType(
          response_headers, request->url(), request->net_log(),
          &data_reduction_proxy_info, event_store, &event_logged);

  if (bypass_type == BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER &&
      DataReductionProxyParams::
          IsIncludedInRemoveMissingViaHeaderOtherBypassFieldTrial()) {
    // Ignore MISSING_VIA_HEADER_OTHER proxy bypass events if the client is part
    // of the field trial to remove these kinds of bypasses.
    bypass_type = BYPASS_EVENT_TYPE_MAX;
  }

  if (proxy_bypass_type)
    *proxy_bypass_type = bypass_type;
  if (bypass_type == BYPASS_EVENT_TYPE_MAX)
    return false;

  if (!event_logged) {
    event_store->AddBypassTypeEvent(
        request->net_log(), bypass_type, request->url(),
        data_reduction_proxy_info.bypass_duration);
  }

  DCHECK(request->context());
  DCHECK(request->context()->proxy_service());
  net::ProxyServer proxy_server;
  SetProxyServerFromGURL(
      data_reduction_proxy_type_info.proxy_servers.first, &proxy_server);

  // Only record UMA if the proxy isn't already on the retry list.
  if (!data_reduction_proxy_params->IsProxyBypassed(
          request->context()->proxy_service()->proxy_retry_info(),
          proxy_server,
          NULL)) {
    DataReductionProxyUsageStats::RecordDataReductionProxyBypassInfo(
        !data_reduction_proxy_type_info.proxy_servers.second.is_empty(),
        data_reduction_proxy_info.bypass_all,
        proxy_server,
        bypass_type);
  }

  if (data_reduction_proxy_info.mark_proxies_as_bad) {
    MarkProxiesAsBadUntil(request,
                          data_reduction_proxy_info.bypass_duration,
                          data_reduction_proxy_info.bypass_all,
                          data_reduction_proxy_type_info.proxy_servers);
  } else {
    request->SetLoadFlags(request->load_flags() |
                          net::LOAD_DISABLE_CACHE |
                          net::LOAD_BYPASS_PROXY);
  }

  // Only retry idempotent methods.
  if (!IsRequestIdempotent(request))
    return false;
  return true;
}

void OnResolveProxyHandler(const GURL& url,
                           int load_flags,
                           const net::ProxyConfig& data_reduction_proxy_config,
                           const net::ProxyConfig& proxy_service_proxy_config,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           const DataReductionProxyParams* params,
                           net::ProxyInfo* result) {
  if (data_reduction_proxy_config.is_valid() &&
      result->proxy_server().is_direct() &&
      !data_reduction_proxy_config.Equals(proxy_service_proxy_config)) {
    net::ProxyInfo data_reduction_proxy_info;
    data_reduction_proxy_config.proxy_rules().Apply(
        url, &data_reduction_proxy_info);
    data_reduction_proxy_info.DeprioritizeBadProxies(proxy_retry_info);
    if (!data_reduction_proxy_info.proxy_server().is_direct())
      result->UseProxyList(data_reduction_proxy_info.proxy_list());
  }

  if ((load_flags & net::LOAD_BYPASS_DATA_REDUCTION_PROXY) &&
      DataReductionProxyParams::IsIncludedInCriticalPathBypassFieldTrial() &&
      !result->is_empty() &&
      !result->is_direct() &&
      params &&
      params->IsDataReductionProxy(
          result->proxy_server().host_port_pair(), NULL)) {
    result->UseDirect();
  }
}

bool IsRequestIdempotent(const net::URLRequest* request) {
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

void MarkProxiesAsBadUntil(
    net::URLRequest* request,
    const base::TimeDelta& bypass_duration,
    bool bypass_all,
    const std::pair<GURL, GURL>& data_reduction_proxies) {
  DCHECK(!data_reduction_proxies.first.is_empty());
  // Synthesize a suitable |ProxyInfo| to add the proxies to the
  // |ProxyRetryInfoMap| of the proxy service.
  net::ProxyList proxy_list;
  net::ProxyServer primary;
  SetProxyServerFromGURL(data_reduction_proxies.first, &primary);
  if (primary.is_valid())
    proxy_list.AddProxyServer(primary);
  net::ProxyServer fallback;
  if (bypass_all) {
    if (!data_reduction_proxies.second.is_empty())
      SetProxyServerFromGURL(data_reduction_proxies.second, &fallback);
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

}  // namespace data_reduction_proxy
