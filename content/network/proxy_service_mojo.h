// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_PROXY_SERVICE_MOJO_H_
#define CONTENT_NETWORK_PROXY_SERVICE_MOJO_H_

#include <memory>

#include "content/common/content_export.h"
#include "net/proxy/dhcp_proxy_script_fetcher.h"

namespace net {
class HostResolver;
class NetLog;
class NetworkDelegate;
class ProxyConfigService;
class ProxyScriptFetcher;
class ProxyService;
}  // namespace net

namespace content {

class MojoProxyResolverFactory;

// Creates a proxy service that uses |mojo_proxy_factory| to create and connect
// to a Mojo proxy resolver service. This proxy service polls
// |proxy_config_service| to notice when the proxy settings change.
//
// |proxy_script_fetcher| specifies the dependency to use for downloading
// any PAC scripts. The resulting ProxyService will take ownership of it.
//
// |dhcp_proxy_script_fetcher| specifies the dependency to use for attempting
// to retrieve the most appropriate PAC script configured in DHCP.
//
// |host_resolver| points to the host resolving dependency the PAC script
// should use for any DNS queries. It must remain valid throughout the
// lifetime of the ProxyService.
std::unique_ptr<net::ProxyService> CONTENT_EXPORT
CreateProxyServiceUsingMojoFactory(
    MojoProxyResolverFactory* mojo_proxy_factory,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::ProxyScriptFetcher* proxy_script_fetcher,
    std::unique_ptr<net::DhcpProxyScriptFetcher> dhcp_proxy_script_fetcher,
    net::HostResolver* host_resolver,
    net::NetLog* net_log,
    net::NetworkDelegate* network_delegate);

}  // namespace content

#endif  // CONTENT_NETWORK_PROXY_SERVICE_MOJO_H_
