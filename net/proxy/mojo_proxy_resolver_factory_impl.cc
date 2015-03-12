// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_factory_impl.h"

#include "net/dns/host_resolver_mojo.h"
#include "net/proxy/mojo_proxy_resolver_impl.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
namespace {

scoped_ptr<ProxyResolver> CreateDefaultProxyResolver(
    HostResolver* host_resolver,
    const ProxyResolver::LoadStateChangedCallback& callback) {
  return make_scoped_ptr(
      new ProxyResolverV8Tracing(host_resolver, nullptr, nullptr, callback));
}

// A class to manage the lifetime of a MojoProxyResolverImpl and a
// HostResolverMojo. An instance will remain while the message pipes for both
// mojo connections remain open.
class MojoProxyResolverHolder : public mojo::ErrorHandler {
 public:
  MojoProxyResolverHolder(
      const MojoProxyResolverFactoryImpl::Factory& proxy_resolver_factory,
      mojo::InterfaceRequest<interfaces::ProxyResolver> request,
      interfaces::HostResolverPtr host_resolver);

 private:
  // mojo::ErrorHandler override.
  void OnConnectionError() override;

  void LoadStateChanged(ProxyResolver::RequestHandle handle,
                        LoadState load_state);

  HostResolverMojo host_resolver_;
  MojoProxyResolverImpl proxy_resolver_;
  mojo::Binding<interfaces::ProxyResolver> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoProxyResolverHolder);
};

MojoProxyResolverHolder::MojoProxyResolverHolder(
    const MojoProxyResolverFactoryImpl::Factory& proxy_resolver_factory,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request,
    interfaces::HostResolverPtr host_resolver)
    : host_resolver_(host_resolver.Pass(),
                     base::Bind(&MojoProxyResolverHolder::OnConnectionError,
                                base::Unretained(this))),
      proxy_resolver_(proxy_resolver_factory.Run(
          &host_resolver_,
          base::Bind(&MojoProxyResolverHolder::LoadStateChanged,
                     base::Unretained(this)))),
      binding_(&proxy_resolver_, request.Pass()) {
  binding_.set_error_handler(this);
}

void MojoProxyResolverHolder::OnConnectionError() {
  delete this;
}

void MojoProxyResolverHolder::LoadStateChanged(
    ProxyResolver::RequestHandle handle,
    LoadState load_state) {
  proxy_resolver_.LoadStateChanged(handle, load_state);
}

}  // namespace

MojoProxyResolverFactoryImpl::MojoProxyResolverFactoryImpl(
    const MojoProxyResolverFactoryImpl::Factory& proxy_resolver_factory,
    mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request)
    : proxy_resolver_impl_factory_(proxy_resolver_factory),
      binding_(this, request.Pass()) {
}

MojoProxyResolverFactoryImpl::MojoProxyResolverFactoryImpl(
    mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request)
    : MojoProxyResolverFactoryImpl(base::Bind(&CreateDefaultProxyResolver),
                                   request.Pass()) {
}

MojoProxyResolverFactoryImpl::~MojoProxyResolverFactoryImpl() = default;

void MojoProxyResolverFactoryImpl::CreateResolver(
    mojo::InterfaceRequest<interfaces::ProxyResolver> request,
    interfaces::HostResolverPtr host_resolver) {
  // The MojoProxyResolverHolder will delete itself when either |request| or
  // |host_resolver| encounters a connection error, that is, when either the
  // ProxyResolver client or the HostResolver implementation is deleted.
  new MojoProxyResolverHolder(proxy_resolver_impl_factory_, request.Pass(),
                              host_resolver.Pass());
}

}  // namespace net
