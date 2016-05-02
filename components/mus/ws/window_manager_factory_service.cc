// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_factory_service.h"

#include "base/bind.h"
#include "components/mus/ws/window_manager_factory_registry.h"
#include "components/mus/ws/window_tree.h"

namespace mus {
namespace ws {

WindowManagerFactoryService::WindowManagerFactoryService(
    WindowManagerFactoryRegistry* registry,
    const UserId& user_id,
    mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request)
    : registry_(registry),
      user_id_(user_id),
      binding_(this, std::move(request)),
      window_manager_factory_(nullptr) {}

WindowManagerFactoryService::~WindowManagerFactoryService() {}

void WindowManagerFactoryService::SetWindowManagerFactory(
    mojom::WindowManagerFactoryPtr factory) {
  window_manager_factory_ptr_ = std::move(factory);
  window_manager_factory_ptr_.set_connection_error_handler(base::Bind(
      &WindowManagerFactoryService::OnConnectionLost, base::Unretained(this)));
  SetWindowManagerFactoryImpl(window_manager_factory_ptr_.get());
}

WindowManagerFactoryService::WindowManagerFactoryService(
    WindowManagerFactoryRegistry* registry,
    const UserId& user_id)
    : registry_(registry),
      user_id_(user_id),
      binding_(this),
      window_manager_factory_(nullptr) {}

void WindowManagerFactoryService::SetWindowManagerFactoryImpl(
    mojom::WindowManagerFactory* factory) {
  DCHECK(!window_manager_factory_);
  window_manager_factory_ = factory;
  registry_->OnWindowManagerFactorySet(this);
}

void WindowManagerFactoryService::OnConnectionLost() {
  registry_->OnWindowManagerFactoryConnectionLost(this);
}

}  // namespace ws
}  // namespace mus
