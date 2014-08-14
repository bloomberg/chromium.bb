// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_protocol.h"

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_tamper_detection.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_headers.h"
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
  *proxy_server = net::ProxyServer(gurl.SchemeIs("http") ?
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
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    DataReductionProxyBypassType* proxy_bypass_type) {
  if (!data_reduction_proxy_params)
    return false;
  DataReductionProxyTypeInfo data_reduction_proxy_type_info;
  if (!data_reduction_proxy_params->WasDataReductionProxyUsed(
          request, &data_reduction_proxy_type_info)) {
    return false;
  }

  // Empty implies either that the request was served from cache or that
  // request was served directly from the origin.
  if (request->proxy_server().IsEmpty())
    return false;

  if (data_reduction_proxy_type_info.proxy_servers.first.is_empty())
    return false;

  DataReductionProxyTamperDetection::DetectAndReport(
      original_response_headers,
      data_reduction_proxy_type_info.proxy_servers.first.SchemeIsSecure());

  DataReductionProxyInfo data_reduction_proxy_info;
  DataReductionProxyBypassType bypass_type =
      GetDataReductionProxyBypassType(original_response_headers,
                                      &data_reduction_proxy_info);
  if (proxy_bypass_type)
    *proxy_bypass_type = bypass_type;
  if (bypass_type == BYPASS_EVENT_TYPE_MAX)
    return false;

  DCHECK(request->context());
  DCHECK(request->context()->proxy_service());
  net::ProxyServer proxy_server;
  SetProxyServerFromGURL(
      data_reduction_proxy_type_info.proxy_servers.first, &proxy_server);

  // Only record UMA if the proxy isn't already on the retry list.
  const net::ProxyRetryInfoMap& proxy_retry_info =
      request->context()->proxy_service()->proxy_retry_info();
  if (proxy_retry_info.find(proxy_server.ToURI()) == proxy_retry_info.end()) {
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
  }

  // Only retry idempotent methods.
  if (!IsRequestIdempotent(request))
    return false;

  OverrideResponseAsRedirect(request,
                             original_response_headers,
                             override_response_headers);
  return true;
}

void OnResolveProxyHandler(const GURL& url,
                           int load_flags,
                           const net::ProxyConfig& data_reduction_proxy_config,
                           const net::ProxyRetryInfoMap& proxy_retry_info,
                           const DataReductionProxyParams* params,
                           net::ProxyInfo* result) {
  if (data_reduction_proxy_config.is_valid() &&
      result->proxy_server().is_direct()) {
    net::ProxyInfo data_reduction_proxy_info;
    data_reduction_proxy_config.proxy_rules().Apply(
        url, &data_reduction_proxy_info);
    data_reduction_proxy_info.DeprioritizeBadProxies(proxy_retry_info);
    result->Use(data_reduction_proxy_info);
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

void OverrideResponseAsRedirect(
    net::URLRequest* request,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  DCHECK(request);
  DCHECK(original_response_headers);
  DCHECK(override_response_headers->get() == NULL);

  request->SetLoadFlags(request->load_flags() |
                        net::LOAD_DISABLE_CACHE |
                        net::LOAD_BYPASS_PROXY);
  *override_response_headers = new net::HttpResponseHeaders(
      original_response_headers->raw_headers());
  (*override_response_headers)->ReplaceStatusLine("HTTP/1.1 302 Found");
  (*override_response_headers)->RemoveHeader("Location");
  (*override_response_headers)->AddHeader("Location: " +
                                          request->url().spec());
  // TODO(bengr): Should we pop_back the request->url_chain?
}

void MarkProxiesAsBadUntil(
    net::URLRequest* request,
    base::TimeDelta& bypass_duration,
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
