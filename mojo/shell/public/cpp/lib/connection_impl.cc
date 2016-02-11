// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/lib/connection_impl.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/interface_binder.h"

namespace mojo {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, public:

ConnectionImpl::ConnectionImpl(
    const std::string& connection_url,
    const std::string& remote_url,
    uint32_t remote_id,
    shell::mojom::InterfaceProviderPtr remote_interfaces,
    shell::mojom::InterfaceProviderRequest local_interfaces,
    const std::set<std::string>& allowed_interfaces)
    : connection_url_(connection_url),
      remote_url_(remote_url),
      remote_id_(remote_id),
      content_handler_id_(0u),
      remote_ids_valid_(false),
      local_binding_(this),
      remote_interfaces_(std::move(remote_interfaces)),
      allowed_interfaces_(allowed_interfaces),
      allow_all_interfaces_(allowed_interfaces_.size() == 1 &&
                            allowed_interfaces_.count("*") == 1),
      default_binder_(nullptr),
      weak_factory_(this) {
  if (local_interfaces.is_pending())
    local_binding_.Bind(std::move(local_interfaces));
}

ConnectionImpl::ConnectionImpl()
    : remote_id_(shell::mojom::Shell::kInvalidApplicationID),
      content_handler_id_(shell::mojom::Shell::kInvalidApplicationID),
      remote_ids_valid_(false),
      local_binding_(this),
      allow_all_interfaces_(true),
      default_binder_(nullptr),
      weak_factory_(this) {
}

ConnectionImpl::~ConnectionImpl() {
  for (auto& i : name_to_binder_)
    delete i.second;
  name_to_binder_.clear();
}

shell::mojom::Shell::ConnectToApplicationCallback
ConnectionImpl::GetConnectToApplicationCallback() {
  return base::Bind(&ConnectionImpl::OnGotRemoteIDs,
                    weak_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, Connection implementation:

void ConnectionImpl::SetDefaultInterfaceBinder(InterfaceBinder* binder) {
  default_binder_ = binder;
}

bool ConnectionImpl::SetInterfaceBinderForName(
    InterfaceBinder* binder,
    const std::string& interface_name) {
  if (allow_all_interfaces_ ||
      allowed_interfaces_.count(interface_name)) {
    RemoveInterfaceBinderForName(interface_name);
    name_to_binder_[interface_name] = binder;
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

shell::mojom::InterfaceProvider* ConnectionImpl::GetRemoteInterfaces() {
  return remote_interfaces_.get();
}

shell::mojom::InterfaceProvider* ConnectionImpl::GetLocalInterfaces() {
  return this;
}

void ConnectionImpl::SetRemoteInterfaceProviderConnectionErrorHandler(
    const Closure& handler) {
  remote_interfaces_.set_connection_error_handler(handler);
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
// ConnectionImpl, shell::mojom::InterfaceProvider implementation:

void ConnectionImpl::GetInterface(const mojo::String& interface_name,
                                  ScopedMessagePipeHandle handle) {
  auto iter = name_to_binder_.find(interface_name);
  InterfaceBinder* binder = iter != name_to_binder_.end() ? iter->second :
      default_binder_;
  if (binder)
    binder->BindInterface(this, interface_name, std::move(handle));
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, private:

void ConnectionImpl::RemoveInterfaceBinderForName(
    const std::string& interface_name) {
  NameToInterfaceBinderMap::iterator it = name_to_binder_.find(interface_name);
  if (it == name_to_binder_.end())
    return;
  delete it->second;
  name_to_binder_.erase(it);
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
