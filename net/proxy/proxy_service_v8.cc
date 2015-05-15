// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service_v8.h"

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "net/proxy/network_delegate_error_observer.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_factory.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"
#include "net/proxy/proxy_service.h"

namespace net {
namespace {

class ProxyResolverFactoryForV8Resolver : public LegacyProxyResolverFactory {
 public:
  explicit ProxyResolverFactoryForV8Resolver(HostResolver* host_resolver,
                                             NetLog* net_log,
                                             NetworkDelegate* network_delegate)
      : LegacyProxyResolverFactory(true),
        host_resolver_(host_resolver),
        net_log_(net_log),
        network_delegate_(network_delegate) {}

  // LegacyProxyResolverFactory override.
  scoped_ptr<ProxyResolver> CreateProxyResolver() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    ProxyResolverErrorObserver* error_observer =
        new NetworkDelegateErrorObserver(
            network_delegate_, base::ThreadTaskRunnerHandle::Get().get());
    return make_scoped_ptr(
        new ProxyResolverV8Tracing(host_resolver_, error_observer, net_log_));
  }

 private:
  HostResolver* const host_resolver_;
  NetLog* const net_log_;
  NetworkDelegate* const network_delegate_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryForV8Resolver);
};
}  // namespace

// static
ProxyService* CreateProxyServiceUsingV8ProxyResolver(
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

  ProxyService* proxy_service =
      new ProxyService(proxy_config_service,
                       make_scoped_ptr(new ProxyResolverFactoryForV8Resolver(
                           host_resolver, net_log, network_delegate)),
                       net_log);

  // Configure fetchers to use for PAC script downloads and auto-detect.
  proxy_service->SetProxyScriptFetchers(proxy_script_fetcher,
                                        dhcp_proxy_script_fetcher);

  return proxy_service;
}

}  // namespace net
