// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/lib/service_registry.h"

#include "base/logging.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/service_connector.h"

namespace mojo {
namespace internal {

ServiceRegistry::ServiceRegistry(
    const std::string& connection_url,
    const std::string& remote_url,
    ServiceProviderPtr remote_services,
    InterfaceRequest<ServiceProvider> local_services,
    const std::set<std::string>& allowed_interfaces)
    : connection_url_(connection_url),
      remote_url_(remote_url),
      local_binding_(this),
      remote_service_provider_(remote_services.Pass()),
      allowed_interfaces_(allowed_interfaces),
      allow_all_interfaces_(allowed_interfaces_.size() == 1 &&
                            allowed_interfaces_.count("*") == 1),
      weak_factory_(this) {
  if (local_services.is_pending())
    local_binding_.Bind(local_services.Pass());
}

ServiceRegistry::ServiceRegistry()
    : local_binding_(this),
      allow_all_interfaces_(true),
      weak_factory_(this) {
}

ServiceRegistry::~ServiceRegistry() {
}

void ServiceRegistry::SetServiceConnector(ServiceConnector* connector) {
  service_connector_registry_.set_service_connector(connector);
}

bool ServiceRegistry::SetServiceConnectorForName(
    ServiceConnector* service_connector,
    const std::string& interface_name) {
  if (allow_all_interfaces_ ||
      allowed_interfaces_.count(interface_name)) {
    service_connector_registry_.SetServiceConnectorForName(service_connector,
                                                           interface_name);
    return true;
  }
  LOG(WARNING) << "CapabilityFilter prevented connection to interface: " <<
      interface_name;
  return false;
}

ServiceProvider* ServiceRegistry::GetLocalServiceProvider() {
  return this;
}

void ServiceRegistry::SetRemoteServiceProviderConnectionErrorHandler(
    const Closure& handler) {
  remote_service_provider_.set_connection_error_handler(handler);
}

base::WeakPtr<ApplicationConnection> ServiceRegistry::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ServiceRegistry::RemoveServiceConnectorForName(
    const std::string& interface_name) {
  service_connector_registry_.RemoveServiceConnectorForName(interface_name);
  if (service_connector_registry_.empty())
    remote_service_provider_.reset();
}

const std::string& ServiceRegistry::GetConnectionURL() {
  return connection_url_;
}

const std::string& ServiceRegistry::GetRemoteApplicationURL() {
  return remote_url_;
}

ServiceProvider* ServiceRegistry::GetServiceProvider() {
  return remote_service_provider_.get();
}

void ServiceRegistry::ConnectToService(const mojo::String& service_name,
                                       ScopedMessagePipeHandle client_handle) {
  service_connector_registry_.ConnectToService(this, service_name,
                                               client_handle.Pass());
}

}  // namespace internal
}  // namespace mojo
