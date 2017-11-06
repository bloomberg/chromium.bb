// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_mojo_proxy_resolver_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

std::unique_ptr<base::ScopedClosureRunner>
TestMojoProxyResolverFactory::CreateResolver(
    const std::string& pac_script,
    mojo::InterfaceRequest<proxy_resolver::mojom::ProxyResolver> req,
    proxy_resolver::mojom::ProxyResolverFactoryRequestClientPtr client) {
  resolver_created_ = true;
  factory_->CreateResolver(pac_script, std::move(req), std::move(client));
  return nullptr;
}

TestMojoProxyResolverFactory::TestMojoProxyResolverFactory()
    : service_ref_factory_(base::Bind(&base::DoNothing)) {
  proxy_resolver_factory_impl_.BindRequest(mojo::MakeRequest(&factory_),
                                           &service_ref_factory_);
}

TestMojoProxyResolverFactory::~TestMojoProxyResolverFactory() = default;

}  // namespace content
