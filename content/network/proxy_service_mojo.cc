// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/proxy_service_mojo.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/network/proxy_resolver_factory_mojo.h"
#include "net/dns/mojo_host_resolver_impl.h"
#include "net/proxy_resolution/network_delegate_error_observer.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "net/proxy_resolution/proxy_service.h"

namespace content {

std::unique_ptr<net::ProxyResolutionService> CreateProxyServiceUsingMojoFactory(
    proxy_resolver::mojom::ProxyResolverFactoryPtr mojo_proxy_factory,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    std::unique_ptr<net::ProxyScriptFetcher> proxy_script_fetcher,
    std::unique_ptr<net::DhcpProxyScriptFetcher> dhcp_proxy_script_fetcher,
    net::HostResolver* host_resolver,
    net::NetLog* net_log,
    net::NetworkDelegate* network_delegate) {
  DCHECK(proxy_config_service);
  DCHECK(proxy_script_fetcher);
  DCHECK(dhcp_proxy_script_fetcher);
  DCHECK(host_resolver);

  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service(
      new net::ProxyResolutionService(
          std::move(proxy_config_service),
          std::make_unique<ProxyResolverFactoryMojo>(
              std::move(mojo_proxy_factory), host_resolver,
              base::Bind(&net::NetworkDelegateErrorObserver::Create,
                         network_delegate, base::ThreadTaskRunnerHandle::Get()),
              net_log),
          net_log));

  // Configure fetchers to use for PAC script downloads and auto-detect.
  proxy_resolution_service->SetProxyScriptFetchers(
      std::move(proxy_script_fetcher),
      std::move(dhcp_proxy_script_fetcher));

  return proxy_resolution_service;
}

}  // namespace content
