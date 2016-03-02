// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_CONNECTION_MANAGER_DELEGATE_H_
#define COMPONENTS_MUS_WS_CONNECTION_MANAGER_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace mus {

namespace mojom {
class Display;
class WindowManagerFactory;
class WindowTree;
}

namespace ws {

class ClientConnection;
class ConnectionManager;
class ServerWindow;
class WindowTreeHostImpl;

class ConnectionManagerDelegate {
 public:
  virtual void OnFirstRootConnectionCreated();

  virtual void OnNoMoreRootConnections() = 0;

  // Creates a ClientConnection in response to Embed() calls on the
  // ConnectionManager.
  virtual ClientConnection* CreateClientConnectionForEmbedAtWindow(
      ConnectionManager* connection_manager,
      mojom::WindowTreeRequest tree_request,
      ServerWindow* root,
      uint32_t policy_bitmask,
      mojom::WindowTreeClientPtr client) = 0;

  virtual ClientConnection* CreateClientConnectionForWindowManager(
      WindowTreeHostImpl* tree_host,
      ServerWindow* window,
      const mojom::Display& display,
      uint32_t user_id,
      mojom::WindowManagerFactory* factory) = 0;

  // Called if no WindowTreeHosts have been created, but a
  // WindowManagerFactory has been set.
  virtual void CreateDefaultWindowTreeHosts() = 0;

 protected:
  virtual ~ConnectionManagerDelegate() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_CONNECTION_MANAGER_DELEGATE_H_
