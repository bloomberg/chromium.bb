// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/forwarding_window_manager.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_host_impl.h"

namespace mus {
namespace ws {

ForwardingWindowManager::ForwardingWindowManager(
    ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {}

ForwardingWindowManager::~ForwardingWindowManager() {}

mojom::WindowManager* ForwardingWindowManager::GetActiveWindowManager() {
  // TODO(sky): This needs to detect active window, or OpenWindow() needs to
  // take the display.
  return connection_manager_->GetActiveWindowTreeHost()->window_manager();
}

void ForwardingWindowManager::OpenWindow(
    mus::mojom::WindowTreeClientPtr client,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) {
  GetActiveWindowManager()->OpenWindow(std::move(client),
                                       std::move(properties));
}

}  // namespace ws
}  // namespace mus
