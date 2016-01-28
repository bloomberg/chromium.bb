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
    : service_(std::move(service)), client_(client) {}

ClientConnection::~ClientConnection() {}

DefaultClientConnection::DefaultClientConnection(
    scoped_ptr<WindowTreeImpl> service_impl,
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojom::WindowTree> service_request,
    mojom::WindowTreeClientPtr client)
    : ClientConnection(std::move(service_impl), client.get()),
      connection_manager_(connection_manager),
      binding_(service(), std::move(service_request)),
      client_(std::move(client)) {
  binding_.set_connection_error_handler(
      [this]() { connection_manager_->OnConnectionError(this); });
}

DefaultClientConnection::~DefaultClientConnection() {}

void DefaultClientConnection::SetIncomingMethodCallProcessingPaused(
    bool paused) {
  if (paused)
    binding_.PauseIncomingMethodCallProcessing();
  else
    binding_.ResumeIncomingMethodCallProcessing();
}

mojom::WindowManager* DefaultClientConnection::GetWindowManager() {
  client_->GetWindowManager(
      GetProxy(&window_manager_internal_, client_.associated_group()));
  return window_manager_internal_.get();
}

}  // namespace ws
}  // namespace mus
