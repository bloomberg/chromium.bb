// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/service_registry_impl.h"

#include <algorithm>
#include <utility>

#include "base/macros.h"
#include "mojo/common/common_type_converters.h"

namespace content {

class ServiceRegistryImpl::ConnectionRequest {
 public:
  ConnectionRequest(const std::string& service_name,
                    mojo::ScopedMessagePipeHandle pipe)
      : service_name_(service_name), pipe_(std::move(pipe)) {
  }
  ~ConnectionRequest() {}

  const std::string& service_name() const { return service_name_; }
  mojo::ScopedMessagePipeHandle PassMessagePipe() { return std::move(pipe_); }

 private:
  const std::string service_name_;
  mojo::ScopedMessagePipeHandle pipe_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionRequest);
};

ServiceRegistryImpl::ServiceRegistryImpl()
    : binding_(this), weak_factory_(this) {
  binding_.set_connection_error_handler([this]() { binding_.Close(); });
}

ServiceRegistryImpl::~ServiceRegistryImpl() {
}

void ServiceRegistryImpl::Bind(
    mojo::InterfaceRequest<RoutedServiceProvider> request) {
  binding_.Bind(std::move(request));
}

void ServiceRegistryImpl::BindRemoteServiceProvider(
    RoutedServiceProviderPtr service_provider) {
  CHECK(!remote_provider_);
  remote_provider_ = std::move(service_provider);

  ConnectionRequests requests;
  std::swap(requests, pending_outgoing_connects_);
  for (auto& request : requests) {
    remote_provider_->ConnectToService(
        request->service_name(), request->PassMessagePipe());
  }

  RoutedConnectionRequestMap request_map;
  std::swap(request_map, pending_routed_outgoing_connects_);
  for (auto& entry : request_map) {
    const int route_id = entry.first;
    for (auto& request : entry.second) {
      remote_provider_->ConnectToServiceRouted(
          route_id, request->service_name(), request->PassMessagePipe());
    }
  }
}

scoped_ptr<ServiceRegistryForRoute>
ServiceRegistryImpl::CreateServiceRegistryForRoute(int route_id) {
  // There should be no previously registered factories for this route ID.
  DCHECK(routed_service_factories_.find(route_id) ==
      routed_service_factories_.end());
  return make_scoped_ptr(new ServiceRegistryForRoute(
      weak_factory_.GetWeakPtr(), route_id,
      base::Bind(&ServiceRegistryImpl::CloseRoute,
                 weak_factory_.GetWeakPtr(), route_id)));
}

void ServiceRegistryImpl::Add(
    const std::string& service_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)> service_factory) {
  service_factories_[service_name] = service_factory;
}

void ServiceRegistryImpl::Remove(const std::string& service_name) {
  service_factories_.erase(service_name);
}

void ServiceRegistryImpl::Connect(const base::StringPiece& service_name,
                                  mojo::ScopedMessagePipeHandle handle) {
  if (!remote_provider_) {
    pending_outgoing_connects_.push_back(make_scoped_ptr(
        new ConnectionRequest(service_name.as_string(), std::move(handle))));
    return;
  }
  remote_provider_->ConnectToService(
      service_name.as_string(), std::move(handle));
}

void ServiceRegistryImpl::ConnectToService(const mojo::String& name,
                                           mojo::ScopedMessagePipeHandle pipe) {
  std::map<std::string,
           base::Callback<void(mojo::ScopedMessagePipeHandle)> >::iterator it =
      service_factories_.find(name);
  if (it == service_factories_.end())
    return;

  // It's possible and effectively unavoidable that under certain conditions
  // an invalid handle may be received. Don't invoke the factory in that case.
  if (!pipe.is_valid()) {
    DVLOG(2) << "Invalid pipe handle for " << name << " interface request.";
    return;
  }

  it->second.Run(std::move(pipe));
}

void ServiceRegistryImpl::ConnectToServiceRouted(
    int32_t route_id,
    const mojo::String& name,
    mojo::ScopedMessagePipeHandle pipe) {
  auto factories_iter = routed_service_factories_.find(route_id);
  if (factories_iter == routed_service_factories_.end()) {
    pending_routed_incoming_connects_[route_id].push_back(make_scoped_ptr(
        new ConnectionRequest(name, std::move(pipe))));
    return;
  }

  ServiceFactoryMap& factories = factories_iter->second;
  auto factory_iter = factories.find(name);
  if (factory_iter == factories.end())
    return;

  if (!pipe.is_valid()) {
    DVLOG(2) << "Invalid pipe handle for " << name << " interface request.";
    return;
  }

  factory_iter->second.Run(std::move(pipe));
}

void ServiceRegistryImpl::AddRouted(
    int route_id,
    const std::string& service_name,
    const ServiceFactory& service_factory) {
  routed_service_factories_[route_id][service_name] = service_factory;
}

void ServiceRegistryImpl::RemoveRouted(int route_id,
                                              const std::string& service_name) {
  routed_service_factories_[route_id].erase(service_name);
}

void ServiceRegistryImpl::ConnectRouted(
    int route_id,
    const std::string& service_name,
    mojo::ScopedMessagePipeHandle handle) {
  if (!remote_provider_) {
    pending_routed_outgoing_connects_[route_id].push_back(make_scoped_ptr(
        new ConnectionRequest(service_name, std::move(handle))));
    return;
  }
  remote_provider_->ConnectToServiceRouted(
      route_id, service_name, std::move(handle));
}

void ServiceRegistryImpl::CloseRoute(int route_id) {
  routed_service_factories_.erase(route_id);
  pending_routed_incoming_connects_.erase(route_id);
  pending_routed_outgoing_connects_.erase(route_id);
}

}  // namespace content
