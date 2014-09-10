// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_delegate.h"

#include "net/base/host_port_pair.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

namespace data_reduction_proxy {

DataReductionProxyDelegate::DataReductionProxyDelegate(
    DataReductionProxyAuthRequestHandler* auth_handler)
    : auth_handler_(auth_handler) {
  DCHECK(auth_handler);
}

DataReductionProxyDelegate::~DataReductionProxyDelegate() {
}

void DataReductionProxyDelegate::OnResolveProxy(
    const GURL& url,
    int load_flags,
    const net::ProxyService& proxy_service,
    net::ProxyInfo* result) {
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
  auth_handler_->MaybeAddProxyTunnelRequestHandler(proxy_server, extra_headers);
}

void DataReductionProxyDelegate::OnTunnelHeadersReceived(
    const net::HostPortPair& origin,
    const net::HostPortPair& proxy_server,
    const net::HttpResponseHeaders& response_headers) {
}

}  // namespace data_reduction_proxy
