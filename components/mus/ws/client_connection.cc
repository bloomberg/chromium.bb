// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/client_connection.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/view_tree_impl.h"

namespace mus {

ClientConnection::ClientConnection(scoped_ptr<ViewTreeImpl> service,
                                   mojo::ViewTreeClient* client)
    : service_(service.Pass()), client_(client) {}

ClientConnection::~ClientConnection() {}

DefaultClientConnection::DefaultClientConnection(
    scoped_ptr<ViewTreeImpl> service_impl,
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewTree> service_request,
    mojo::ViewTreeClientPtr client)
    : ClientConnection(service_impl.Pass(), client.get()),
      connection_manager_(connection_manager),
      binding_(service(), service_request.Pass()),
      client_(client.Pass()) {
  binding_.set_connection_error_handler(
      [this]() { connection_manager_->OnConnectionError(this); });
}

DefaultClientConnection::~DefaultClientConnection() {}

}  // namespace mus
