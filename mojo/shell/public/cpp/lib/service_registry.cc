// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/lib/service_registry.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/service_connector.h"

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
      remote_service_provider_(std::move(remote_services)),
      allowed_interfaces_(allowed_interfaces),
      allow_all_interfaces_(allowed_interfaces_.size() == 1 &&
                            allowed_interfaces_.count("*") == 1),
      content_handler_id_(0u),
      is_content_handler_id_valid_(false),
      weak_factory_(this) {
  if (local_services.is_pending())
    local_binding_.Bind(std::move(local_services));
}

ServiceRegistry::ServiceRegistry()
    : local_binding_(this),
      allow_all_interfaces_(true),
      weak_factory_(this) {
}

ServiceRegistry::~ServiceRegistry() {
}

Shell::ConnectToApplicationCallback
ServiceRegistry::GetConnectToApplicationCallback() {
  return base::Bind(&ServiceRegistry::OnGotContentHandlerID,
                    weak_factory_.GetWeakPtr());
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
  LOG(WARNING) << "CapabilityFilter prevented connection to interface: "
               << interface_name << " connection_url:" << connection_url_
               << " remote_url:" << remote_url_;
  return false;
}

ServiceProvider* ServiceRegistry::GetLocalServiceProvider() {
  return this;
}

void ServiceRegistry::SetRemoteServiceProviderConnectionErrorHandler(
    const Closure& handler) {
  remote_service_provider_.set_connection_error_handler(handler);
}

bool ServiceRegistry::GetContentHandlerID(uint32_t* content_handler_id) {
  if (!is_content_handler_id_valid_)
    return false;

  *content_handler_id = content_handler_id_;
  return true;
}

void ServiceRegistry::AddContentHandlerIDCallback(const Closure& callback) {
  if (is_content_handler_id_valid_) {
    callback.Run();
    return;
  }
  content_handler_id_callbacks_.push_back(callback);
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

void ServiceRegistry::OnGotContentHandlerID(uint32_t content_handler_id) {
  DCHECK(!is_content_handler_id_valid_);
  is_content_handler_id_valid_ = true;
  content_handler_id_ = content_handler_id;
  std::vector<Closure> callbacks;
  callbacks.swap(content_handler_id_callbacks_);
  for (auto callback : callbacks)
    callback.Run();
}

void ServiceRegistry::ConnectToService(const mojo::String& service_name,
                                       ScopedMessagePipeHandle client_handle) {
  service_connector_registry_.ConnectToService(this, service_name,
                                               std::move(client_handle));
}

}  // namespace internal
}  // namespace mojo
