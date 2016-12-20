// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"

namespace data_reduction_proxy {

DataReductionProxyServer::DataReductionProxyServer(
    const net::ProxyServer& proxy_server,
    ProxyServer_ProxyType proxy_type)
    : proxy_server_(proxy_server), proxy_type_(proxy_type) {}

bool DataReductionProxyServer::operator==(
    const DataReductionProxyServer& other) const {
  return proxy_server_ == other.proxy_server_ &&
         proxy_type_ == other.proxy_type_;
}

// static
std::vector<net::ProxyServer>
DataReductionProxyServer::ConvertToNetProxyServers(
    const std::vector<DataReductionProxyServer>& data_reduction_proxy_servers) {
  std::vector<net::ProxyServer> net_proxy_servers;
  for (auto data_reduction_proxy_server : data_reduction_proxy_servers)
    net_proxy_servers.push_back(data_reduction_proxy_server.proxy_server());
  return net_proxy_servers;
}

ProxyServer_ProxyType DataReductionProxyServer::GetProxyTypeForTesting() const {
  return proxy_type_;
}

}  // namespace data_reduction_proxy
