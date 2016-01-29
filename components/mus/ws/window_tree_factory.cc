// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_factory.h"

#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {

WindowTreeFactory::WindowTreeFactory(ConnectionManager* connection_manager)
    : connection_manager_(connection_manager) {}

WindowTreeFactory::~WindowTreeFactory() {}

void WindowTreeFactory::AddBinding(
    mojo::InterfaceRequest<mus::mojom::WindowTreeFactory> request) {
  binding_.AddBinding(this, std::move(request));
}

void WindowTreeFactory::CreateWindowTree(
    mojo::InterfaceRequest<mojom::WindowTree> tree_request,
    mojom::WindowTreeClientPtr client) {
  scoped_ptr<ws::WindowTreeImpl> service(new ws::WindowTreeImpl(
      connection_manager_, nullptr, mojom::WindowTree::kAccessPolicyDefault));
  scoped_ptr<ws::DefaultClientConnection> client_connection(
      new ws::DefaultClientConnection(std::move(service), connection_manager_,
                                      std::move(tree_request),
                                      std::move(client)));
  connection_manager_->AddConnection(std::move(client_connection), nullptr);
}

}  // namespace ws
}  // namespace mus
