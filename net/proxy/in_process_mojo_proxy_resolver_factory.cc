// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/in_process_mojo_proxy_resolver_factory.h"

#include "base/memory/singleton.h"
#include "net/proxy/mojo_proxy_resolver_factory_impl.h"

namespace net {

// static
InProcessMojoProxyResolverFactory*
InProcessMojoProxyResolverFactory::GetInstance() {
  return Singleton<InProcessMojoProxyResolverFactory>::get();
}

InProcessMojoProxyResolverFactory::InProcessMojoProxyResolverFactory() {
  // Implementation lifetime is strongly bound to the life of the connection via
  // |factory_|. When |factory_| is destroyed, the Mojo connection is terminated
  // which causes this object to be destroyed.
  new MojoProxyResolverFactoryImpl(mojo::GetProxy(&factory_));
}

InProcessMojoProxyResolverFactory::~InProcessMojoProxyResolverFactory() =
    default;

void InProcessMojoProxyResolverFactory::Create(
    mojo::InterfaceRequest<interfaces::ProxyResolver> req,
    interfaces::HostResolverPtr host_resolver) {
  factory_->CreateResolver(req.Pass(), host_resolver.Pass());
}

}  // namespace net
