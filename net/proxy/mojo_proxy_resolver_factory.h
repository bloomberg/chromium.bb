// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_H_
#define NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_H_

#include "net/interfaces/host_resolver_service.mojom.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace net {

// Factory for connecting to Mojo ProxyResolver services.
class MojoProxyResolverFactory {
 public:
  // Connect to a new ProxyResolver service using request |req|, using
  // |host_resolver| as the DNS resolver.
  // Note: The connection request |req| may be resolved asynchronously.
  virtual void Create(mojo::InterfaceRequest<interfaces::ProxyResolver> req,
                      interfaces::HostResolverPtr host_resolver) = 0;
};

}  // namespace net

#endif  // NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_H_
