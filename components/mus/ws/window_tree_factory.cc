// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_factory.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"

namespace mus {
namespace ws {

WindowTreeFactory::WindowTreeFactory(ConnectionManager* connection_manager,
                                     const UserId& user_id)
    : connection_manager_(connection_manager), user_id_(user_id) {}

WindowTreeFactory::~WindowTreeFactory() {}

void WindowTreeFactory::AddBinding(
    mojo::InterfaceRequest<mus::mojom::WindowTreeFactory> request) {
  binding_.AddBinding(this, std::move(request));
}

void WindowTreeFactory::CreateWindowTree(
    mojo::InterfaceRequest<mojom::WindowTree> tree_request,
    mojom::WindowTreeClientPtr client) {
  scoped_ptr<ws::WindowTree> service(
      new ws::WindowTree(connection_manager_, user_id_, nullptr,
                         mojom::WindowTree::kAccessPolicyDefault));
  scoped_ptr<ws::DefaultWindowTreeBinding> binding(
      new ws::DefaultWindowTreeBinding(service.get(), connection_manager_,
                                       std::move(tree_request),
                                       std::move(client)));
  connection_manager_->AddTree(std::move(service), std::move(binding), nullptr);
}

}  // namespace ws
}  // namespace mus
