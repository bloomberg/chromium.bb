// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/lib/connection_impl.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/service_connector.h"

namespace mojo {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, public:

ConnectionImpl::ConnectionImpl(
    const std::string& connection_url,
    const std::string& remote_url,
    uint32_t remote_id,
    ServiceProviderPtr remote_services,
    InterfaceRequest<ServiceProvider> local_services,
    const std::set<std::string>& allowed_interfaces)
    : connection_url_(connection_url),
      remote_url_(remote_url),
      remote_id_(remote_id),
      content_handler_id_(0u),
      remote_ids_valid_(false),
      local_binding_(this),
      remote_service_provider_(std::move(remote_services)),
      allowed_interfaces_(allowed_interfaces),
      allow_all_interfaces_(allowed_interfaces_.size() == 1 &&
                            allowed_interfaces_.count("*") == 1),
      default_connector_(nullptr),
      weak_factory_(this) {
  if (local_services.is_pending())
    local_binding_.Bind(std::move(local_services));
}

ConnectionImpl::ConnectionImpl()
    : remote_id_(shell::mojom::Shell::kInvalidApplicationID),
      content_handler_id_(shell::mojom::Shell::kInvalidApplicationID),
      remote_ids_valid_(false),
      local_binding_(this),
      allow_all_interfaces_(true),
      default_connector_(nullptr),
      weak_factory_(this) {
}

ConnectionImpl::~ConnectionImpl() {
  for (NameToServiceConnectorMap::iterator i =
           name_to_service_connector_.begin();
       i != name_to_service_connector_.end(); ++i) {
    delete i->second;
  }
  name_to_service_connector_.clear();
}

shell::mojom::Shell::ConnectToApplicationCallback
ConnectionImpl::GetConnectToApplicationCallback() {
  return base::Bind(&ConnectionImpl::OnGotRemoteIDs,
                    weak_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, Connection implementation:

void ConnectionImpl::SetServiceConnector(ServiceConnector* service_connector) {
  default_connector_ = service_connector;
}

bool ConnectionImpl::SetServiceConnectorForName(
    ServiceConnector* service_connector,
    const std::string& interface_name) {
  if (allow_all_interfaces_ ||
      allowed_interfaces_.count(interface_name)) {
    RemoveServiceConnectorForName(interface_name);
    name_to_service_connector_[interface_name] = service_connector;
    return true;
  }
  LOG(WARNING) << "CapabilityFilter prevented connection to interface: "
               << interface_name << " connection_url:" << connection_url_
               << " remote_url:" << remote_url_;
  return false;
}

const std::string& ConnectionImpl::GetConnectionURL() {
  return connection_url_;
}

const std::string& ConnectionImpl::GetRemoteApplicationURL() {
  return remote_url_;
}

ServiceProvider* ConnectionImpl::GetServiceProvider() {
  return remote_service_provider_.get();
}

ServiceProvider* ConnectionImpl::GetLocalServiceProvider() {
  return this;
}

void ConnectionImpl::SetRemoteServiceProviderConnectionErrorHandler(
    const Closure& handler) {
  remote_service_provider_.set_connection_error_handler(handler);
}

bool ConnectionImpl::GetRemoteApplicationID(uint32_t* remote_id) const {
  if (!remote_ids_valid_)
    return false;

  *remote_id = remote_id_;
  return true;
}

bool ConnectionImpl::GetRemoteContentHandlerID(
    uint32_t* content_handler_id) const {
  if (!remote_ids_valid_)
    return false;

  *content_handler_id = content_handler_id_;
  return true;
}

void ConnectionImpl::AddRemoteIDCallback(const Closure& callback) {
  if (remote_ids_valid_) {
    callback.Run();
    return;
  }
  remote_id_callbacks_.push_back(callback);
}

base::WeakPtr<Connection> ConnectionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, ServiceProvider implementation:

void ConnectionImpl::ConnectToService(const mojo::String& interface_name,
                                      ScopedMessagePipeHandle client_handle) {
  auto iter = name_to_service_connector_.find(interface_name);
  if (iter != name_to_service_connector_.end()) {
    iter->second->ConnectToService(this, interface_name,
                                   std::move(client_handle));
  }
  if (default_connector_) {
    default_connector_->ConnectToService(this, interface_name,
                                         std::move(client_handle));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, private:

void ConnectionImpl::RemoveServiceConnectorForName(
    const std::string& interface_name) {
  NameToServiceConnectorMap::iterator it =
      name_to_service_connector_.find(interface_name);
  if (it == name_to_service_connector_.end())
    return;
  delete it->second;
  name_to_service_connector_.erase(it);
}

void ConnectionImpl::OnGotRemoteIDs(uint32_t target_application_id,
                                     uint32_t content_handler_id) {
  DCHECK(!remote_ids_valid_);
  remote_ids_valid_ = true;

  remote_id_ = target_application_id;
  content_handler_id_ = content_handler_id;
  std::vector<Closure> callbacks;
  callbacks.swap(remote_id_callbacks_);
  for (auto callback : callbacks)
    callback.Run();
}

}  // namespace internal
}  // namespace mojo
