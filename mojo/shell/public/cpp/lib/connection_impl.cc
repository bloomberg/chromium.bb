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
    const std::string& connection_name,
    const std::string& remote_name,
    uint32_t remote_id,
    uint32_t remote_user_id,
    shell::mojom::InterfaceProviderPtr remote_interfaces,
    shell::mojom::InterfaceProviderRequest local_interfaces,
    const std::set<std::string>& allowed_interfaces)
    : connection_name_(connection_name),
      remote_name_(remote_name),
      remote_id_(remote_id),
      remote_ids_valid_(false),
      remote_user_id_(remote_user_id),
      local_registry_(std::move(local_interfaces), this),
      remote_interfaces_(std::move(remote_interfaces)),
      allowed_interfaces_(allowed_interfaces),
      allow_all_interfaces_(allowed_interfaces_.size() == 1 &&
                            allowed_interfaces_.count("*") == 1),
      weak_factory_(this) {}

ConnectionImpl::ConnectionImpl()
    : remote_id_(shell::mojom::Connector::kInvalidApplicationID),
      remote_ids_valid_(false),
      local_registry_(shell::mojom::InterfaceProviderRequest(), this),
      allow_all_interfaces_(true),
      weak_factory_(this) {}

ConnectionImpl::~ConnectionImpl() {}

shell::mojom::Connector::ConnectCallback ConnectionImpl::GetConnectCallback() {
  return base::Bind(&ConnectionImpl::OnGotInstanceID,
                    weak_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, Connection implementation:

const std::string& ConnectionImpl::GetConnectionName() {
  return connection_name_;
}

const std::string& ConnectionImpl::GetRemoteApplicationName() {
  return remote_name_;
}

uint32_t ConnectionImpl::GetRemoteUserID() const {
  return remote_user_id_;
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

void ConnectionImpl::AddRemoteIDCallback(const Closure& callback) {
  if (remote_ids_valid_) {
    callback.Run();
    return;
  }
  remote_id_callbacks_.push_back(callback);
}

bool ConnectionImpl::AllowsInterface(const std::string& interface_name) const {
  return allow_all_interfaces_ || allowed_interfaces_.count(interface_name);
}

shell::mojom::InterfaceProvider* ConnectionImpl::GetRemoteInterfaces() {
  return remote_interfaces_.get();
}

InterfaceRegistry* ConnectionImpl::GetLocalRegistry() {
  return &local_registry_;
}

base::WeakPtr<Connection> ConnectionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, private:

void ConnectionImpl::OnGotInstanceID(uint32_t target_application_id) {
  DCHECK(!remote_ids_valid_);
  remote_ids_valid_ = true;

  remote_id_ = target_application_id;
  std::vector<Closure> callbacks;
  callbacks.swap(remote_id_callbacks_);
  for (auto callback : callbacks)
    callback.Run();
}

}  // namespace internal
}  // namespace mojo
