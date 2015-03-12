// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_IMPL_H_
#define NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_IMPL_H_

#include "base/callback.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/proxy_resolver.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace net {
class HostResolver;

class MojoProxyResolverFactoryImpl : public interfaces::ProxyResolverFactory {
 public:
  using Factory = base::Callback<scoped_ptr<ProxyResolver>(
      HostResolver*,
      const ProxyResolver::LoadStateChangedCallback&)>;

  explicit MojoProxyResolverFactoryImpl(
      mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request);
  MojoProxyResolverFactoryImpl(
      const Factory& proxy_resolver_factory,
      mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request);

  ~MojoProxyResolverFactoryImpl() override;

 private:
  // interfaces::ProxyResolverFactory override.
  void CreateResolver(mojo::InterfaceRequest<interfaces::ProxyResolver> request,
                      interfaces::HostResolverPtr host_resolver) override;

  const Factory proxy_resolver_impl_factory_;
  mojo::StrongBinding<interfaces::ProxyResolverFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoProxyResolverFactoryImpl);
};

}  // namespace net

#endif  // NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_IMPL_H_
