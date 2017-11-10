// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_PROXY_RESOLVER_FACTORY_MOJO_H_
#define CONTENT_NETWORK_PROXY_RESOLVER_FACTORY_MOJO_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_resolver_factory.h"
#include "services/proxy_resolver/public/interfaces/proxy_resolver.mojom.h"

namespace net {
class HostResolver;
class NetLog;
class ProxyResolverErrorObserver;
class ProxyResolverScriptData;
}  // namespace net

namespace content {

// Implementation of ProxyResolverFactory that connects to a Mojo service to
// create implementations of a Mojo proxy resolver to back a ProxyResolverMojo.
class CONTENT_EXPORT ProxyResolverFactoryMojo
    : public net::ProxyResolverFactory {
 public:
  ProxyResolverFactoryMojo(
      proxy_resolver::mojom::ProxyResolverFactoryPtr mojo_proxy_factory,
      net::HostResolver* host_resolver,
      const base::Callback<std::unique_ptr<net::ProxyResolverErrorObserver>()>&
          error_observer_factory,
      net::NetLog* net_log);
  ~ProxyResolverFactoryMojo() override;

  // ProxyResolverFactory override.
  int CreateProxyResolver(
      const scoped_refptr<net::ProxyResolverScriptData>& pac_script,
      std::unique_ptr<net::ProxyResolver>* resolver,
      const net::CompletionCallback& callback,
      std::unique_ptr<Request>* request) override;

 private:
  class Job;

  proxy_resolver::mojom::ProxyResolverFactoryPtr mojo_proxy_factory_;
  net::HostResolver* const host_resolver_;
  const base::Callback<std::unique_ptr<net::ProxyResolverErrorObserver>()>
      error_observer_factory_;
  net::NetLog* const net_log_;

  base::WeakPtrFactory<ProxyResolverFactoryMojo> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryMojo);
};

}  // namespace content

#endif  // CONTENT_NETWORK_PROXY_RESOLVER_FACTORY_MOJO_H_
