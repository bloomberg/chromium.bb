// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_host_connection.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {

WindowTreeHostConnectionImpl::WindowTreeHostConnectionImpl(
    mojo::InterfaceRequest<mojom::WindowTreeHost> request,
    WindowTreeHostImpl* host_impl,
    mojom::WindowTreeClientPtr client,
    ConnectionManager* manager)
    : connection_manager_(manager),
      binding_(host_impl, std::move(request)),
      client_(std::move(client)) {}

WindowTreeHostConnectionImpl::~WindowTreeHostConnectionImpl() {}

WindowTreeImpl* WindowTreeHostConnectionImpl::CreateWindowTree(
    ServerWindow* root) {
  WindowTreeImpl* tree = connection_manager_->EmbedAtWindow(
      root, mojom::WindowTree::kAccessPolicyEmbedRoot, std::move(client_));
  tree->ConfigureWindowManager();
  return tree;
}

}  // namespace ws
}  // namespace mus
