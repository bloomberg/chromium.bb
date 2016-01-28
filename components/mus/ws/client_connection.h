// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_CLIENT_CONNECTION_H_
#define COMPONENTS_MUS_WS_CLIENT_CONNECTION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class ConnectionManager;
class WindowTreeImpl;

// ClientConnection encapsulates the state needed for a single client connected
// to the window manager.
class ClientConnection {
 public:
  ClientConnection(scoped_ptr<WindowTreeImpl> service,
                   mojom::WindowTreeClient* client);
  virtual ~ClientConnection();

  WindowTreeImpl* service() { return service_.get(); }
  const WindowTreeImpl* service() const { return service_.get(); }

  mojom::WindowTreeClient* client() { return client_; }

  virtual mojom::WindowManager* GetWindowManager() = 0;

  virtual void SetIncomingMethodCallProcessingPaused(bool paused) = 0;

 private:
  scoped_ptr<WindowTreeImpl> service_;
  mojom::WindowTreeClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ClientConnection);
};

// Bindings implementation of ClientConnection.
class DefaultClientConnection : public ClientConnection {
 public:
  DefaultClientConnection(
      scoped_ptr<WindowTreeImpl> service_impl,
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojom::WindowTree> service_request,
      mojom::WindowTreeClientPtr client);
  ~DefaultClientConnection() override;

  // ClientConnection:
  mojom::WindowManager* GetWindowManager() override;
  void SetIncomingMethodCallProcessingPaused(bool paused) override;

 private:
  ConnectionManager* connection_manager_;
  mojo::Binding<mojom::WindowTree> binding_;
  mojom::WindowTreeClientPtr client_;
  mojom::WindowManagerAssociatedPtr window_manager_internal_;

  DISALLOW_COPY_AND_ASSIGN(DefaultClientConnection);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_CLIENT_CONNECTION_H_
