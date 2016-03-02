// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_factory_service.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {

WindowManagerFactoryService::WindowManagerFactoryService(
    WindowManagerFactoryRegistry* registry,
    uint32_t user_id,
    mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request)
    : registry_(registry),
      user_id_(user_id),
      binding_(this, std::move(request)) {}

WindowManagerFactoryService::~WindowManagerFactoryService() {}

void WindowManagerFactoryService::SetWindowManagerFactory(
    mojom::WindowManagerFactoryPtr factory) {
  window_manager_factory_ = std::move(factory);
  registry_->OnWindowManagerFactorySet();
  binding_.set_connection_error_handler(base::Bind(
      &WindowManagerFactoryService::OnConnectionLost, base::Unretained(this)));
}

void WindowManagerFactoryService::OnConnectionLost() {
  registry_->OnWindowManagerFactoryConnectionLost(this);
}

}  // namespace ws
}  // namespace mus
