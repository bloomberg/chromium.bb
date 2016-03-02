// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_factory_registry.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_manager_factory_service.h"

namespace mus {
namespace ws {

WindowManagerFactoryRegistry::WindowManagerFactoryRegistry(
    ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {}

WindowManagerFactoryRegistry::~WindowManagerFactoryRegistry() {}

void WindowManagerFactoryRegistry::Register(
    uint32_t user_id,
    mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request) {
  for (auto& service_ptr : services_) {
    if (service_ptr->user_id() == user_id) {
      LOG(ERROR) << "WindowManagerFactoryService already registered for "
                 << user_id;
      return;
    }
  }

  scoped_ptr<WindowManagerFactoryService> service(
      new WindowManagerFactoryService(this, user_id, std::move(request)));
  services_.push_back(std::move(service));
}

std::vector<WindowManagerFactoryService*>
WindowManagerFactoryRegistry::GetServices() {
  std::vector<WindowManagerFactoryService*> result;
  for (auto& service_ptr : services_)
    result.push_back(service_ptr.get());
  return result;
}

void WindowManagerFactoryRegistry::OnWindowManagerFactoryConnectionLost(
    WindowManagerFactoryService* service) {
  for (auto it = services_.begin(); it != services_.end(); ++it) {
    if (it->get() == service) {
      services_.erase(it);
      return;
    }
  }
}

void WindowManagerFactoryRegistry::OnWindowManagerFactorySet() {
  connection_manager_->OnWindowManagerFactorySet();
}

}  // namespace ws
}  // namespace mus
