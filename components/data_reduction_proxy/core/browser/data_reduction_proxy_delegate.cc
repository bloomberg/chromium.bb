// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"

#include <cmath>

#include "base/metrics/sparse_histogram.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_server.h"

namespace data_reduction_proxy {

DataReductionProxyDelegate::DataReductionProxyDelegate(
    DataReductionProxyRequestOptions* request_options,
    DataReductionProxyConfig* config)
    : request_options_(request_options),
      config_(config) {
  DCHECK(request_options);
  DCHECK(config);
}

DataReductionProxyDelegate::~DataReductionProxyDelegate() {
}

void DataReductionProxyDelegate::OnResolveProxy(
    const GURL& url,
    int load_flags,
    const net::ProxyService& proxy_service,
    net::ProxyInfo* result) {
}

void DataReductionProxyDelegate::OnTunnelConnectCompleted(
    const net::HostPortPair& endpoint,
    const net::HostPortPair& proxy_server,
    int net_error) {
  if (config_->IsDataReductionProxy(proxy_server, NULL)) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("DataReductionProxy.HTTPConnectCompleted",
                                std::abs(net_error));
  }
}

void DataReductionProxyDelegate::OnFallback(const net::ProxyServer& bad_proxy,
                                            int net_error) {
}

void DataReductionProxyDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
}

void DataReductionProxyDelegate::OnBeforeTunnelRequest(
    const net::HostPortPair& proxy_server,
    net::HttpRequestHeaders* extra_headers) {
  request_options_->MaybeAddProxyTunnelRequestHandler(
      proxy_server, extra_headers);
}

bool DataReductionProxyDelegate::IsTrustedSpdyProxy(
    const net::ProxyServer& proxy_server) {
  if (!proxy_server.is_https() ||
      !params::IsIncludedInTrustedSpdyProxyFieldTrial() ||
      !proxy_server.is_valid()) {
    return false;
  }
  return config_ &&
         config_->IsDataReductionProxy(proxy_server.host_port_pair(), nullptr);
}

void DataReductionProxyDelegate::OnTunnelHeadersReceived(
    const net::HostPortPair& origin,
    const net::HostPortPair& proxy_server,
    const net::HttpResponseHeaders& response_headers) {
}

}  // namespace data_reduction_proxy
