// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/interface_registry.h"

#include "mojo/shell/public/cpp/connection.h"

namespace mojo {

InterfaceRegistry::InterfaceRegistry(Connection* connection)
    : InterfaceRegistry(GetProxy(&client_handle_), connection) {}

InterfaceRegistry::InterfaceRegistry(
    shell::mojom::InterfaceProviderRequest request,
    Connection* connection)
    : binding_(this),
      connection_(connection),
      default_binder_(nullptr) {
  if (request.is_pending())
    binding_.Bind(std::move(request));
}

InterfaceRegistry::~InterfaceRegistry() {
  for (auto& i : name_to_binder_)
    delete i.second;
  name_to_binder_.clear();
}

shell::mojom::InterfaceProviderPtr InterfaceRegistry::TakeClientHandle() {
  return std::move(client_handle_);
}

// shell::mojom::InterfaceProvider:
void InterfaceRegistry::GetInterface(const String& interface_name,
                                     ScopedMessagePipeHandle handle) {
  auto iter = name_to_binder_.find(interface_name);
  InterfaceBinder* binder = iter != name_to_binder_.end() ? iter->second :
      default_binder_;
  if (binder)
    binder->BindInterface(connection_, interface_name, std::move(handle));
}

bool InterfaceRegistry::SetInterfaceBinderForName(
    InterfaceBinder* binder,
    const std::string& interface_name) {
  if (!connection_ ||
      (connection_ && connection_->AllowsInterface(interface_name))) {
    RemoveInterfaceBinderForName(interface_name);
    name_to_binder_[interface_name] = binder;
    return true;
  }
  LOG(WARNING) << "Connection CapabilityFilter prevented binding to interface: "
               << interface_name << " connection_url:"
               << connection_->GetConnectionURL() << " remote_url:"
               << connection_->GetRemoteApplicationURL();
  return false;
}

void InterfaceRegistry::RemoveInterfaceBinderForName(
    const std::string& interface_name) {
  NameToInterfaceBinderMap::iterator it = name_to_binder_.find(interface_name);
  if (it == name_to_binder_.end())
    return;
  delete it->second;
  name_to_binder_.erase(it);
}

}  // namespace mojo
