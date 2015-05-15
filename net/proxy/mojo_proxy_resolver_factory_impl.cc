// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_factory_impl.h"

#include <string>

#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_mojo.h"
#include "net/proxy/mojo_proxy_resolver_impl.h"
#include "net/proxy/proxy_resolver_error_observer.h"
#include "net/proxy/proxy_resolver_factory.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
namespace {

scoped_ptr<ProxyResolverErrorObserver> ReturnNullErrorObserver() {
  return nullptr;
}

scoped_ptr<ProxyResolverFactory> CreateDefaultProxyResolver(
    HostResolver* host_resolver,
    const ProxyResolver::LoadStateChangedCallback& callback) {
  return make_scoped_ptr(new ProxyResolverFactoryV8Tracing(
      host_resolver, nullptr, callback, base::Bind(&ReturnNullErrorObserver)));
}

class LoadStateChangeForwarder
    : public base::RefCounted<LoadStateChangeForwarder> {
 public:
  LoadStateChangeForwarder() = default;

  void OnLoadStateChanged(ProxyResolver::RequestHandle request_handle,
                          LoadState load_state) const {
    if (!callback_.is_null())
      callback_.Run(request_handle, load_state);
  }

  void set_load_state_changed_callback(
      const ProxyResolver::LoadStateChangedCallback& callback) {
    callback_ = callback;
  }

 private:
  friend class base::RefCounted<LoadStateChangeForwarder>;
  ~LoadStateChangeForwarder() = default;

  ProxyResolver::LoadStateChangedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(LoadStateChangeForwarder);
};

// A class to manage the lifetime of a MojoProxyResolverImpl and a
// HostResolverMojo. An instance will remain while the message pipes for both
// mojo connections remain open.
class MojoProxyResolverHolder : public mojo::ErrorHandler {
 public:
  MojoProxyResolverHolder(
      scoped_ptr<HostResolverMojo> host_resolver,
      scoped_ptr<ProxyResolver> proxy_resolver_impl,
      const scoped_refptr<LoadStateChangeForwarder>&
          load_state_change_forwarder,
      mojo::InterfaceRequest<interfaces::ProxyResolver> request);

 private:
  // mojo::ErrorHandler override.
  void OnConnectionError() override;

  scoped_ptr<HostResolverMojo> host_resolver_;
  MojoProxyResolverImpl mojo_proxy_resolver_;
  mojo::Binding<interfaces::ProxyResolver> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoProxyResolverHolder);
};

MojoProxyResolverHolder::MojoProxyResolverHolder(
    scoped_ptr<HostResolverMojo> host_resolver,
    scoped_ptr<ProxyResolver> proxy_resolver_impl,
    const scoped_refptr<LoadStateChangeForwarder>& load_state_change_forwarder,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request)
    : host_resolver_(host_resolver.Pass()),
      mojo_proxy_resolver_(
          proxy_resolver_impl.Pass(),
          base::Bind(&LoadStateChangeForwarder::set_load_state_changed_callback,
                     load_state_change_forwarder)),
      binding_(&mojo_proxy_resolver_, request.Pass()) {
  binding_.set_error_handler(this);
  host_resolver_->set_disconnect_callback(base::Bind(
      &MojoProxyResolverHolder::OnConnectionError, base::Unretained(this)));
}

void MojoProxyResolverHolder::OnConnectionError() {
  delete this;
}

}  // namespace

class MojoProxyResolverFactoryImpl::Job : public mojo::ErrorHandler {
 public:
  Job(MojoProxyResolverFactoryImpl* parent,
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      const MojoProxyResolverFactoryImpl::Factory& proxy_resolver_factory,
      mojo::InterfaceRequest<interfaces::ProxyResolver> request,
      interfaces::HostResolverPtr host_resolver,
      interfaces::ProxyResolverFactoryRequestClientPtr client);
  ~Job() override;

 private:
  // mojo::ErrorHandler override.
  void OnConnectionError() override;

  void OnProxyResolverCreated(int error);

  MojoProxyResolverFactoryImpl* const parent_;
  scoped_ptr<HostResolverMojo> host_resolver_;
  scoped_refptr<LoadStateChangeForwarder> load_state_change_forwarder_;
  scoped_ptr<ProxyResolver> proxy_resolver_impl_;
  mojo::InterfaceRequest<interfaces::ProxyResolver> proxy_request_;
  scoped_ptr<net::ProxyResolverFactory> factory_;
  scoped_ptr<net::ProxyResolverFactory::Request> request_;
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

MojoProxyResolverFactoryImpl::Job::Job(
    MojoProxyResolverFactoryImpl* factory,
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    const MojoProxyResolverFactoryImpl::Factory& proxy_resolver_factory,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request,
    interfaces::HostResolverPtr host_resolver,
    interfaces::ProxyResolverFactoryRequestClientPtr client)
    : parent_(factory),
      host_resolver_(new HostResolverMojo(
          host_resolver.Pass(),
          base::Bind(&MojoProxyResolverFactoryImpl::Job::OnConnectionError,
                     base::Unretained(this)))),
      load_state_change_forwarder_(new LoadStateChangeForwarder),
      proxy_request_(request.Pass()),
      factory_(proxy_resolver_factory.Run(
          host_resolver_.get(),
          base::Bind(&LoadStateChangeForwarder::OnLoadStateChanged,
                     load_state_change_forwarder_))),
      client_ptr_(client.Pass()) {
  client_ptr_.set_error_handler(this);
  factory_->CreateProxyResolver(
      pac_script, &proxy_resolver_impl_,
      base::Bind(&MojoProxyResolverFactoryImpl::Job::OnProxyResolverCreated,
                 base::Unretained(this)),
      &request_);
}

MojoProxyResolverFactoryImpl::Job::~Job() = default;

void MojoProxyResolverFactoryImpl::Job::OnConnectionError() {
  client_ptr_->ReportResult(ERR_PAC_SCRIPT_TERMINATED);
  parent_->RemoveJob(this);
}

void MojoProxyResolverFactoryImpl::Job::OnProxyResolverCreated(int error) {
  if (error == OK) {
    // The MojoProxyResolverHolder will delete itself if either
    // |host_resolver_| or |proxy_request_| encounters a connection error.
    new MojoProxyResolverHolder(
        host_resolver_.Pass(), proxy_resolver_impl_.Pass(),
        load_state_change_forwarder_, proxy_request_.Pass());
  }
  client_ptr_->ReportResult(error);
  parent_->RemoveJob(this);
}

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

MojoProxyResolverFactoryImpl::~MojoProxyResolverFactoryImpl() {
  STLDeleteElements(&jobs_);
}

void MojoProxyResolverFactoryImpl::CreateResolver(
    const mojo::String& pac_script,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request,
    interfaces::HostResolverPtr host_resolver,
    interfaces::ProxyResolverFactoryRequestClientPtr client) {
  // The Job will call RemoveJob on |this| when either the create request
  // finishes or |request| or |client| encounters a connection error.
  jobs_.insert(new Job(
      this, ProxyResolverScriptData::FromUTF8(pac_script.To<std::string>()),
      proxy_resolver_impl_factory_, request.Pass(), host_resolver.Pass(),
      client.Pass()));
}

void MojoProxyResolverFactoryImpl::RemoveJob(Job* job) {
  size_t erased = jobs_.erase(job);
  DCHECK_EQ(1u, erased);
  delete job;
}

}  // namespace net
