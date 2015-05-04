// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_UTILITY_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_
#define CHROME_BROWSER_NET_UTILITY_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_

#include "base/macros.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"

template <typename Type>
struct DefaultSingletonTraits;

// A factory used to create connections to Mojo proxy resolver services run in a
// utility process. All Mojo proxy resolver services will be run in the same
// utility process. Utility process crashes are detected and the utility
// process is automatically restarted.
class UtilityProcessMojoProxyResolverFactory
    : public net::interfaces::ProxyResolverFactory,
      public mojo::ErrorHandler {
 public:
  static UtilityProcessMojoProxyResolverFactory* GetInstance();

  // Overridden from net::interfaces::ProxyResolverFactory:
  void CreateResolver(
      const mojo::String& pac_script,
      mojo::InterfaceRequest<net::interfaces::ProxyResolver> req,
      net::interfaces::HostResolverPtr host_resolver,
      net::interfaces::ProxyResolverFactoryRequestClientPtr client) override;

 private:
  friend struct DefaultSingletonTraits<UtilityProcessMojoProxyResolverFactory>;
  UtilityProcessMojoProxyResolverFactory();
  ~UtilityProcessMojoProxyResolverFactory() override;

  // Overridden from mojo::ErrorHandler:
  void OnConnectionError() override;

  // Creates a new utility process and connects to its Mojo proxy resolver
  // factory.
  void CreateProcessAndConnect();

  net::interfaces::ProxyResolverFactoryPtr resolver_factory_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessMojoProxyResolverFactory);
};

#endif  // CHROME_BROWSER_NET_UTILITY_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_
