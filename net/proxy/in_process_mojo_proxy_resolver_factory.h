// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_IN_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_
#define NET_PROXY_IN_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_

#include "base/macros.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"

template <typename T>
struct DefaultSingletonTraits;

namespace net {

// Factory to connect to an in-process Mojo proxy resolver service.
// NOTE: This is intended to be temporary for debugging purposes and will be
// removed when we're confident with the out-of-process implementation.
class InProcessMojoProxyResolverFactory : public MojoProxyResolverFactory {
 public:
  static InProcessMojoProxyResolverFactory* GetInstance();

  // Overridden from MojoProxyResolverFactory:
  void Create(mojo::InterfaceRequest<interfaces::ProxyResolver> req,
              interfaces::HostResolverPtr host_resolver) override;

 private:
  InProcessMojoProxyResolverFactory();
  ~InProcessMojoProxyResolverFactory() override;
  friend struct DefaultSingletonTraits<InProcessMojoProxyResolverFactory>;

  interfaces::ProxyResolverFactoryPtr factory_;

  DISALLOW_COPY_AND_ASSIGN(InProcessMojoProxyResolverFactory);
};

}  // namespace net

#endif  // NET_PROXY_IN_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_
