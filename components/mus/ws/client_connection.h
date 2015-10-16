// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_CLIENT_CONNECTION_H_
#define COMPONENTS_MUS_WS_CLIENT_CONNECTION_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mus {

class ConnectionManager;
class ViewTreeImpl;

// ClientConnection encapsulates the state needed for a single client connected
// to the view manager.
class ClientConnection {
 public:
  ClientConnection(scoped_ptr<ViewTreeImpl> service,
                   mojo::ViewTreeClient* client);
  virtual ~ClientConnection();

  ViewTreeImpl* service() { return service_.get(); }
  const ViewTreeImpl* service() const { return service_.get(); }

  mojo::ViewTreeClient* client() { return client_; }

 private:
  scoped_ptr<ViewTreeImpl> service_;
  mojo::ViewTreeClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ClientConnection);
};

// Bindings implementation of ClientConnection.
class DefaultClientConnection : public ClientConnection {
 public:
  DefaultClientConnection(
      scoped_ptr<ViewTreeImpl> service_impl,
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewTree> service_request,
      mojo::ViewTreeClientPtr client);
  ~DefaultClientConnection() override;

 private:
  ConnectionManager* connection_manager_;
  mojo::Binding<mojo::ViewTree> binding_;
  mojo::ViewTreeClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(DefaultClientConnection);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_CLIENT_CONNECTION_H_
