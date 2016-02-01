// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_factory_service.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {

WindowManagerFactoryService::WindowManagerFactoryService(
    mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request)
    : binding_(this, std::move(request)) {}

WindowManagerFactoryService::~WindowManagerFactoryService() {}

void WindowManagerFactoryService::SetWindowManagerFactory(
    mojom::WindowManagerFactoryPtr factory) {
  window_manager_factory_ = std::move(factory);
}

}  // namespace ws
}  // namespace mus
