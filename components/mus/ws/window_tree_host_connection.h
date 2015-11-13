// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_HOST_CONNECTION_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_HOST_CONNECTION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/window_tree_host_delegate.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {

namespace ws {

class ConnectionManager;
class WindowTreeImpl;

// WindowTreeHostConnection is a server-side object that encapsulates the
// connection between a client of the WindowTreeHost and its implementation.
// WindowTreeHostConnection also establishes a ClientConnection via the same
// code path as embedded windows. ConnectionManager manages the lifetime of
// WindowTreeHostConnections. If a connection to the client is lost or if the
// associated root is closed, the WindowTreeHostConnection will inform the
// ConnectionManager to destroy the root and its associated window tree.
class WindowTreeHostConnection : public WindowTreeHostDelegate {
 public:
  WindowTreeHostConnection(scoped_ptr<WindowTreeHostImpl> host_impl,
                           ConnectionManager* connection_manager);

  void set_window_tree(WindowTreeImpl* tree) { tree_ = tree; }

  WindowTreeHostImpl* window_tree_host() { return host_.get(); }
  const WindowTreeHostImpl* window_tree_host() const { return host_.get(); }

  ConnectionManager* connection_manager() { return connection_manager_; }
  const ConnectionManager* connection_manager() const {
    return connection_manager_;
  }

  void CloseConnection();

 protected:
  ~WindowTreeHostConnection() override;

  // WindowTreeHostDelegate:
  void OnDisplayInitialized() override;
  void OnDisplayClosed() override;
  WindowTreeImpl* GetWindowTree() override;

 private:
  scoped_ptr<WindowTreeHostImpl> host_;
  WindowTreeImpl* tree_;
  ConnectionManager* connection_manager_;
  bool connection_closed_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostConnection);
};

// Live implementation of WindowTreeHostConnection.
class WindowTreeHostConnectionImpl : public WindowTreeHostConnection {
 public:
  WindowTreeHostConnectionImpl(
      mojo::InterfaceRequest<mojom::WindowTreeHost> request,
      scoped_ptr<WindowTreeHostImpl> host_impl,
      mojom::WindowTreeClientPtr client,
      ConnectionManager* connection_manager);

 private:
  ~WindowTreeHostConnectionImpl() override;

  // WindowTreeHostDelegate:
  void OnDisplayInitialized() override;

  mojo::Binding<mojom::WindowTreeHost> binding_;
  mojom::WindowTreeClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostConnectionImpl);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_HOST_CONNECTION_H_
