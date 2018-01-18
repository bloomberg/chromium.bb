// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/network/url_request_context_builder_mojo.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/network/network_context.h"
#include "content/network/proxy_service_mojo.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"

namespace content {

URLRequestContextBuilderMojo::URLRequestContextBuilderMojo()
    : dhcp_fetcher_factory_(new net::DhcpProxyScriptFetcherFactory()) {}

URLRequestContextBuilderMojo::~URLRequestContextBuilderMojo() = default;

void URLRequestContextBuilderMojo::SetDhcpFetcherFactory(
    std::unique_ptr<net::DhcpProxyScriptFetcherFactory> dhcp_fetcher_factory) {
  dhcp_fetcher_factory_ = std::move(dhcp_fetcher_factory);
}

void URLRequestContextBuilderMojo::SetMojoProxyResolverFactory(
    proxy_resolver::mojom::ProxyResolverFactoryPtr
        mojo_proxy_resolver_factory) {
  mojo_proxy_resolver_factory_ = std::move(mojo_proxy_resolver_factory);
}

URLRequestContextOwner URLRequestContextBuilderMojo::Create(
    network::mojom::NetworkContextParams* params,
    bool quic_disabled,
    net::NetLog* net_log) {
  return NetworkContext::ApplyContextParamsToBuilder(this, params,
                                                     quic_disabled, net_log);
}

std::unique_ptr<net::ProxyService>
URLRequestContextBuilderMojo::CreateProxyService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::URLRequestContext* url_request_context,
    net::HostResolver* host_resolver,
    net::NetworkDelegate* network_delegate,
    net::NetLog* net_log) {
  DCHECK(url_request_context);
  DCHECK(host_resolver);

  if (!mojo_proxy_resolver_factory_) {
    return net::URLRequestContextBuilder::CreateProxyService(
        std::move(proxy_config_service), url_request_context, host_resolver,
        network_delegate, net_log);
  }

  std::unique_ptr<net::DhcpProxyScriptFetcher> dhcp_proxy_script_fetcher =
      dhcp_fetcher_factory_->Create(url_request_context);
  std::unique_ptr<net::ProxyScriptFetcher> proxy_script_fetcher =
      std::make_unique<net::ProxyScriptFetcherImpl>(url_request_context);
  return CreateProxyServiceUsingMojoFactory(
      std::move(mojo_proxy_resolver_factory_), std::move(proxy_config_service),
      std::move(proxy_script_fetcher), std::move(dhcp_proxy_script_fetcher),
      host_resolver, net_log, network_delegate);
}

}  // namespace content
