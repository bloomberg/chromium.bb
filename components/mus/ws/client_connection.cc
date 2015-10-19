// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/client_connection.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {

namespace ws {

ClientConnection::ClientConnection(scoped_ptr<WindowTreeImpl> service,
                                   mojom::WindowTreeClient* client)
    : service_(service.Pass()), client_(client) {}

ClientConnection::~ClientConnection() {}

DefaultClientConnection::DefaultClientConnection(
    scoped_ptr<WindowTreeImpl> service_impl,
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojom::WindowTree> service_request,
    mojom::WindowTreeClientPtr client)
    : ClientConnection(service_impl.Pass(), client.get()),
      connection_manager_(connection_manager),
      binding_(service(), service_request.Pass()),
      client_(client.Pass()) {
  binding_.set_connection_error_handler(
      [this]() { connection_manager_->OnConnectionError(this); });
}

DefaultClientConnection::~DefaultClientConnection() {}

}  // namespace ws

}  // namespace mus
