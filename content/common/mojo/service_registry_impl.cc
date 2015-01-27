// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/service_registry_impl.h"

#include "mojo/common/common_type_converters.h"

namespace content {

ServiceRegistryImpl::ServiceRegistryImpl()
    : binding_(this), weak_factory_(this) {
}

ServiceRegistryImpl::~ServiceRegistryImpl() {
  while (!pending_connects_.empty()) {
    mojo::CloseRaw(pending_connects_.front().second);
    pending_connects_.pop();
  }
}

void ServiceRegistryImpl::Bind(
    mojo::InterfaceRequest<mojo::ServiceProvider> request) {
  binding_.Bind(request.Pass());
}

void ServiceRegistryImpl::BindRemoteServiceProvider(
    mojo::ServiceProviderPtr service_provider) {
  CHECK(!remote_provider_);
  remote_provider_ = service_provider.Pass();
  while (!pending_connects_.empty()) {
    remote_provider_->ConnectToService(
        mojo::String::From(pending_connects_.front().first),
        mojo::ScopedMessagePipeHandle(pending_connects_.front().second));
    pending_connects_.pop();
  }
}

void ServiceRegistryImpl::AddService(
    const std::string& service_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)> service_factory) {
  service_factories_[service_name] = service_factory;
}

void ServiceRegistryImpl::RemoveService(const std::string& service_name) {
  service_factories_.erase(service_name);
}

void ServiceRegistryImpl::ConnectToRemoteService(
    const base::StringPiece& service_name,
    mojo::ScopedMessagePipeHandle handle) {
  if (!remote_provider_) {
    pending_connects_.push(
        std::make_pair(service_name.as_string(), handle.release()));
    return;
  }
  remote_provider_->ConnectToService(mojo::String::From(service_name),
                                     handle.Pass());
}

base::WeakPtr<ServiceRegistry> ServiceRegistryImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ServiceRegistryImpl::ConnectToService(
    const mojo::String& name,
    mojo::ScopedMessagePipeHandle client_handle) {
  std::map<std::string,
           base::Callback<void(mojo::ScopedMessagePipeHandle)> >::iterator it =
      service_factories_.find(name);
  if (it == service_factories_.end())
    return;

  it->second.Run(client_handle.Pass());
}

}  // namespace content
