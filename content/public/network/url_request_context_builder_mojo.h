// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
#define CONTENT_PUBLIC_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/network/url_request_context_owner.h"
#include "net/proxy/dhcp_proxy_script_fetcher_factory.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/public/interfaces/network_service.mojom.h"
#include "services/proxy_resolver/public/interfaces/proxy_resolver.mojom.h"

namespace net {
class HostResolver;
class NetLog;
class NetworkDelegate;
class ProxyService;
class URLRequestContext;
}  // namespace net

namespace content {

// Specialization of URLRequestContextBuilder that can create a ProxyService
// that uses a Mojo ProxyResolver. The consumer is responsible for providing
// the proxy_resolver::mojom::ProxyResolverFactory.  If a ProxyService is set
// directly via the URLRequestContextBuilder API, it will be used instead.
class CONTENT_EXPORT URLRequestContextBuilderMojo
    : public net::URLRequestContextBuilder {
 public:
  URLRequestContextBuilderMojo();
  ~URLRequestContextBuilderMojo() override;

  // Overrides default DhcpProxyScriptFetcherFactory. Ignored if no
  // proxy_resolver::mojom::ProxyResolverFactory is provided.
  void SetDhcpFetcherFactory(
      std::unique_ptr<net::DhcpProxyScriptFetcherFactory> dhcp_fetcher_factory);

  // Sets Mojo factory used to create ProxyResolvers. If not set, falls back to
  // URLRequestContext's default behavior.
  void SetMojoProxyResolverFactory(
      proxy_resolver::mojom::ProxyResolverFactoryPtr
          mojo_proxy_resolver_factory);

  // Can be used to create a URLRequestContext from this consumer-configured
  // URLRequestContextBuilder, which |params| will then be applied to. The
  // results URLRequestContext will be returned along with other state that it
  // depends on. The URLRequestContext can be further modified before first use.
  //
  // This method is intended to ease the transition to an out-of-process
  // NetworkService, and will be removed once that ships.
  URLRequestContextOwner Create(network::mojom::NetworkContextParams* params,
                                bool quic_disabled,
                                net::NetLog* net_log);

 private:
  std::unique_ptr<net::ProxyService> CreateProxyService(
      std::unique_ptr<net::ProxyConfigService> proxy_config_service,
      net::URLRequestContext* url_request_context,
      net::HostResolver* host_resolver,
      net::NetworkDelegate* network_delegate,
      net::NetLog* net_log) override;

  std::unique_ptr<net::DhcpProxyScriptFetcherFactory> dhcp_fetcher_factory_;

  proxy_resolver::mojom::ProxyResolverFactoryPtr mojo_proxy_resolver_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextBuilderMojo);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
