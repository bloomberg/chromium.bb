// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_MOJO_PROXY_RESOLVER_FACTORY_H_
#define CONTENT_TEST_TEST_MOJO_PROXY_RESOLVER_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/network/mojo_proxy_resolver_factory.h"

namespace content {

// MojoProxyResolverFactory that runs PAC scripts in-process, for tests.
class TestMojoProxyResolverFactory : public MojoProxyResolverFactory {
 public:
  TestMojoProxyResolverFactory();
  ~TestMojoProxyResolverFactory() override;

  // Returns true if CreateResolver was called.
  bool resolver_created() const { return resolver_created_; }

  // Overridden from MojoProxyResolverFactory:
  std::unique_ptr<base::ScopedClosureRunner> CreateResolver(
      const std::string& pac_script,
      mojo::InterfaceRequest<proxy_resolver::mojom::ProxyResolver> req,
      proxy_resolver::mojom::ProxyResolverFactoryRequestClientPtr client)
      override;

 private:
  proxy_resolver::mojom::ProxyResolverFactoryPtr factory_;

  bool resolver_created_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestMojoProxyResolverFactory);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_MOJO_PROXY_RESOLVER_FACTORY_H_
