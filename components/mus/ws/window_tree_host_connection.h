// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_HOST_CONNECTION_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_HOST_CONNECTION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class ConnectionManager;
class ServerWindow;
class WindowTreeImpl;

// WindowTreeHostConnection encapsulates the connection between a client of the
// WindowTreeHost and its implementation. Additionally WindowTreeHostConnection
// is able to create the WindowTree that is bound to the root.
//
// WindowTreeHostConnection is owned by WindowTreeHostImpl.
class WindowTreeHostConnection {
 public:
  virtual ~WindowTreeHostConnection() {}

  virtual WindowTreeImpl* CreateWindowTree(ServerWindow* root) = 0;
};

// Live implementation of WindowTreeHostConnection.
class WindowTreeHostConnectionImpl : public WindowTreeHostConnection {
 public:
  WindowTreeHostConnectionImpl(
      mojo::InterfaceRequest<mojom::WindowTreeHost> request,
      WindowTreeHostImpl* host_impl,
      mojom::WindowTreeClientPtr client,
      ConnectionManager* connection_manager);
  ~WindowTreeHostConnectionImpl() override;

 private:
  // WindowTreeHostConnection:
  WindowTreeImpl* CreateWindowTree(ServerWindow* root) override;

  ConnectionManager* connection_manager_;
  mojo::Binding<mojom::WindowTreeHost> binding_;
  mojom::WindowTreeClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostConnectionImpl);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_HOST_CONNECTION_H_
