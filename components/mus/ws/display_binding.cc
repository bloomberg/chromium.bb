// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/display_binding.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {

DisplayBindingImpl::DisplayBindingImpl(mojom::WindowTreeHostRequest request,
                                       Display* display,
                                       mojom::WindowTreeClientPtr client,
                                       ConnectionManager* manager)
    : connection_manager_(manager),
      binding_(display, std::move(request)),
      client_(std::move(client)) {}

DisplayBindingImpl::~DisplayBindingImpl() {}

WindowTreeImpl* DisplayBindingImpl::CreateWindowTree(ServerWindow* root) {
  WindowTreeImpl* tree = connection_manager_->EmbedAtWindow(
      root, mojom::WindowTree::kAccessPolicyEmbedRoot, std::move(client_));
  tree->ConfigureWindowManager();
  return tree;
}

}  // namespace ws
}  // namespace mus
