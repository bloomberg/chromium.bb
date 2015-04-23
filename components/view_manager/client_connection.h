// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_CLIENT_CONNECTION_H_
#define COMPONENTS_VIEW_MANAGER_CLIENT_CONNECTION_H_

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace view_manager {

class ConnectionManager;
class ViewManagerServiceImpl;

// ClientConnection encapsulates the state needed for a single client connected
// to the view manager.
class ClientConnection {
 public:
  ClientConnection(scoped_ptr<ViewManagerServiceImpl> service,
                   mojo::ViewManagerClient* client);
  virtual ~ClientConnection();

  ViewManagerServiceImpl* service() { return service_.get(); }
  const ViewManagerServiceImpl* service() const { return service_.get(); }

  mojo::ViewManagerClient* client() { return client_; }

 private:
  scoped_ptr<ViewManagerServiceImpl> service_;
  mojo::ViewManagerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ClientConnection);
};

// Bindings implementation of ClientConnection.
class DefaultClientConnection : public ClientConnection,
                                public mojo::ErrorHandler {
 public:
  DefaultClientConnection(
      scoped_ptr<ViewManagerServiceImpl> service_impl,
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ViewManagerClientPtr client);
  ~DefaultClientConnection() override;

 private:
  // ErrorHandler:
  void OnConnectionError() override;

  ConnectionManager* connection_manager_;
  mojo::Binding<mojo::ViewManagerService> binding_;
  mojo::ViewManagerClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(DefaultClientConnection);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_CLIENT_CONNECTION_H_
