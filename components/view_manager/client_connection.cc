// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/client_connection.h"

#include "components/view_manager/connection_manager.h"
#include "components/view_manager/view_manager_service_impl.h"

namespace view_manager {

ClientConnection::ClientConnection(scoped_ptr<ViewManagerServiceImpl> service,
                                   mojo::ViewManagerClient* client)
    : service_(service.Pass()), client_(client) {
}

ClientConnection::~ClientConnection() {
}

DefaultClientConnection::DefaultClientConnection(
    scoped_ptr<ViewManagerServiceImpl> service_impl,
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
    mojo::ViewManagerClientPtr client)
    : ClientConnection(service_impl.Pass(), client.get()),
      connection_manager_(connection_manager),
      binding_(service(), service_request.Pass()),
      client_(client.Pass()) {
  binding_.set_error_handler(this);
}

DefaultClientConnection::~DefaultClientConnection() {
}

void DefaultClientConnection::OnConnectionError() {
  connection_manager_->OnConnectionError(this);
}

}  // namespace view_manager
