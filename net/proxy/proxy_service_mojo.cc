// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service_mojo.h"

#include "base/logging.h"
#include "net/dns/host_resolver_mojo.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/in_process_mojo_proxy_resolver_factory.h"
#include "net/proxy/mojo_proxy_resolver_impl.h"
#include "net/proxy/proxy_resolver_mojo.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"
#include "net/proxy/proxy_service.h"

namespace net {

ProxyService* CreateProxyServiceUsingMojoFactory(
    MojoProxyResolverFactory* mojo_proxy_factory,
    ProxyConfigService* proxy_config_service,
    ProxyScriptFetcher* proxy_script_fetcher,
    DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher,
    HostResolver* host_resolver,
    NetLog* net_log,
    NetworkDelegate* network_delegate) {
  DCHECK(proxy_config_service);
  DCHECK(proxy_script_fetcher);
  DCHECK(dhcp_proxy_script_fetcher);
  DCHECK(host_resolver);

  ProxyResolverMojo* proxy_resolver =
      new ProxyResolverMojo(mojo_proxy_factory, host_resolver);

  ProxyService* proxy_service =
      new ProxyService(proxy_config_service, proxy_resolver, net_log);

  // Configure fetchers to use for PAC script downloads and auto-detect.
  proxy_service->SetProxyScriptFetchers(proxy_script_fetcher,
                                        dhcp_proxy_script_fetcher);

  return proxy_service;
}

ProxyService* CreateProxyServiceUsingMojoInProcess(
    ProxyConfigService* proxy_config_service,
    ProxyScriptFetcher* proxy_script_fetcher,
    DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher,
    HostResolver* host_resolver,
    NetLog* net_log,
    NetworkDelegate* network_delegate) {
  return CreateProxyServiceUsingMojoFactory(
      InProcessMojoProxyResolverFactory::GetInstance(), proxy_config_service,
      proxy_script_fetcher, dhcp_proxy_script_fetcher, host_resolver, net_log,
      network_delegate);
}

}  // namespace net
