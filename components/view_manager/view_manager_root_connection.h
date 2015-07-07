// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_CONNECTION_H_
#define COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_CONNECTION_H_

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "components/view_manager/view_manager_root_delegate.h"
#include "components/view_manager/view_manager_root_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace view_manager {

class ConnectionManager;
class ViewManagerServiceImpl;

// ViewManagerRootConnection is a server-side object that encapsulates the
// connection between a client of the ViewManagerRoot and its implementation.
// ViewManagerRootConnection also establishes a ClientConnection via the same
// code path as embedded views. ConnectionManager manages the lifetime of
// ViewManagerRootConnections. If a connection to the client is lost or if the
// associated root is closed, the ViewManagerRootConnection will inform the
// ConnectionManager to destroy the root and its associated view tree.
class ViewManagerRootConnection : public ViewManagerRootDelegate {
 public:
  ViewManagerRootConnection(
      scoped_ptr<ViewManagerRootImpl> view_manager_root,
      ConnectionManager* connection_manager);

  void set_view_manager_service(ViewManagerServiceImpl* service) {
    service_ = service;
  }

  ViewManagerRootImpl* view_manager_root() { return root_.get();}
  const ViewManagerRootImpl* view_manager_root() const { return root_.get(); }

  ConnectionManager* connection_manager() { return connection_manager_; }
  const ConnectionManager* connection_manager() const {
    return connection_manager_;
  }

  void CloseConnection();

 protected:
  ~ViewManagerRootConnection() override;

 private:
  // ViewManagerRootDelegate:
  ViewManagerServiceImpl* GetViewManagerService() override;
  void OnDisplayClosed() override;

  scoped_ptr<ViewManagerRootImpl> root_;
  ViewManagerServiceImpl* service_;
  ConnectionManager* connection_manager_;
  bool connection_closed_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerRootConnection);
};

// Live implementation of ViewManagerRootConnection.
class ViewManagerRootConnectionImpl : public mojo::ErrorHandler,
                                      public ViewManagerRootConnection {
 public:
  ViewManagerRootConnectionImpl(
      mojo::InterfaceRequest<mojo::ViewManagerRoot> request,
      scoped_ptr<ViewManagerRootImpl> root,
      mojo::ViewManagerClientPtr client,
      ConnectionManager* connection_manager);

 private:
  ~ViewManagerRootConnectionImpl() override;

  // ErrorHandler:
  void OnConnectionError() override;

  mojo::Binding<mojo::ViewManagerRoot> binding_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerRootConnectionImpl);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_ROOT_CONNECTION_H_
