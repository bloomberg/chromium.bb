// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/client_connection.h"

#include "base/bind.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {
namespace ws {

ClientConnection::ClientConnection(mojom::WindowTreeClient* client)
    : client_(client) {}

ClientConnection::~ClientConnection() {}

DefaultClientConnection::DefaultClientConnection(
    WindowTreeImpl* tree,
    ConnectionManager* connection_manager,
    mojom::WindowTreeRequest service_request,
    mojom::WindowTreeClientPtr client)
    : ClientConnection(client.get()),
      connection_manager_(connection_manager),
      binding_(tree, std::move(service_request)),
      client_(std::move(client)) {
  // Both |connection_manager| and |tree| outlive us.
  binding_.set_connection_error_handler(
      base::Bind(&ConnectionManager::DestroyTree,
                 base::Unretained(connection_manager), base::Unretained(tree)));
}

DefaultClientConnection::DefaultClientConnection(
    WindowTreeImpl* tree,
    ConnectionManager* connection_manager,
    mojom::WindowTreeClientPtr client)
    : ClientConnection(client.get()),
      connection_manager_(connection_manager),
      binding_(tree),
      client_(std::move(client)) {}

DefaultClientConnection::~DefaultClientConnection() {}

void DefaultClientConnection::SetIncomingMethodCallProcessingPaused(
    bool paused) {
  if (paused)
    binding_.PauseIncomingMethodCallProcessing();
  else
    binding_.ResumeIncomingMethodCallProcessing();
}

mojom::WindowTreePtr DefaultClientConnection::CreateInterfacePtrAndBind() {
  DCHECK(!binding_.is_bound());
  return binding_.CreateInterfacePtrAndBind();
}

mojom::WindowManager* DefaultClientConnection::GetWindowManager() {
  client_->GetWindowManager(
      GetProxy(&window_manager_internal_, client_.associated_group()));
  return window_manager_internal_.get();
}

}  // namespace ws
}  // namespace mus
