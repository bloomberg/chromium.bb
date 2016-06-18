// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/service_registry_impl.h"

#include <utility>

#include "mojo/common/common_type_converters.h"

namespace content {

ServiceRegistry* ServiceRegistry::Create() {
  return new ServiceRegistryImpl;
}

ServiceRegistryImpl::ServiceRegistryImpl()
    : binding_(this),
      weak_factory_(this) {
  remote_provider_request_ = GetProxy(&remote_provider_);
}

ServiceRegistryImpl::~ServiceRegistryImpl() {
}

void ServiceRegistryImpl::Bind(shell::mojom::InterfaceProviderRequest request) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(base::Bind(
      &ServiceRegistryImpl::OnConnectionError, base::Unretained(this)));
}

shell::mojom::InterfaceProviderRequest
ServiceRegistryImpl::TakeRemoteRequest() {
  return std::move(remote_provider_request_);
}

void ServiceRegistryImpl::AddService(
    const std::string& service_name,
    const ServiceFactory& service_factory,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  service_factories_[service_name] =
      std::make_pair(service_factory, task_runner);
}

void ServiceRegistryImpl::RemoveService(const std::string& service_name) {
  service_factories_.erase(service_name);
}

void ServiceRegistryImpl::ConnectToRemoteService(
    base::StringPiece service_name,
    mojo::ScopedMessagePipeHandle handle) {
  auto override_it = service_overrides_.find(service_name.as_string());
  if (override_it != service_overrides_.end()) {
    override_it->second.Run(std::move(handle));
    return;
  }
  remote_provider_->GetInterface(
      mojo::String::From(service_name.as_string()), std::move(handle));
}

void ServiceRegistryImpl::AddServiceOverrideForTesting(
    const std::string& service_name,
    const ServiceFactory& factory) {
  service_overrides_[service_name] = factory;
}

void ServiceRegistryImpl::ClearServiceOverridesForTesting() {
  service_overrides_.clear();
}

bool ServiceRegistryImpl::IsBound() const {
  return binding_.is_bound();
}

base::WeakPtr<ServiceRegistry> ServiceRegistryImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ServiceRegistryImpl::GetInterface(
    const mojo::String& name,
    mojo::ScopedMessagePipeHandle client_handle) {
  auto it = service_factories_.find(name);
  if (it == service_factories_.end()) {
    DLOG(ERROR) << name << " not found";
    return;
  }

  // It's possible and effectively unavoidable that under certain conditions
  // an invalid handle may be received. Don't invoke the factory in that case.
  if (!client_handle.is_valid()) {
    DVLOG(2) << "Invalid pipe handle for " << name << " interface request.";
    return;
  }

  if (it->second.second) {
    it->second.second->PostTask(
        FROM_HERE,
        base::Bind(&ServiceRegistryImpl::RunServiceFactoryOnTaskRunner,
                   it->second.first, base::Passed(&client_handle)));
    return;
  }
  it->second.first.Run(std::move(client_handle));
}

// static
void ServiceRegistryImpl::RunServiceFactoryOnTaskRunner(
    const ServiceRegistryImpl::ServiceFactory& factory,
    mojo::ScopedMessagePipeHandle handle) {
  factory.Run(std::move(handle));
}

void ServiceRegistryImpl::OnConnectionError() {
  binding_.Close();
}

}  // namespace content
