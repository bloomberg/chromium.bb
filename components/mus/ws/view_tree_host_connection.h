// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_VIEW_TREE_HOST_CONNECTION_H_
#define COMPONENTS_MUS_WS_VIEW_TREE_HOST_CONNECTION_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "components/mus/ws/view_tree_host_delegate.h"
#include "components/mus/ws/view_tree_host_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mus {

class ConnectionManager;
class ViewTreeImpl;

// ViewTreeHostConnection is a server-side object that encapsulates the
// connection between a client of the ViewTreeHost and its implementation.
// ViewTreeHostConnection also establishes a ClientConnection via the same
// code path as embedded views. ConnectionManager manages the lifetime of
// ViewTreeHostConnections. If a connection to the client is lost or if the
// associated root is closed, the ViewTreeHostConnection will inform the
// ConnectionManager to destroy the root and its associated view tree.
class ViewTreeHostConnection : public ViewTreeHostDelegate {
 public:
  ViewTreeHostConnection(scoped_ptr<ViewTreeHostImpl> host_impl,
                         ConnectionManager* connection_manager);

  void set_view_tree(ViewTreeImpl* tree) { tree_ = tree; }

  ViewTreeHostImpl* view_tree_host() { return host_.get(); }
  const ViewTreeHostImpl* view_tree_host() const { return host_.get(); }

  ConnectionManager* connection_manager() { return connection_manager_; }
  const ConnectionManager* connection_manager() const {
    return connection_manager_;
  }

  void CloseConnection();

 protected:
  ~ViewTreeHostConnection() override;

  // ViewTreeHostDelegate:
  void OnDisplayInitialized() override;
  void OnDisplayClosed() override;
  ViewTreeImpl* GetViewTree() override;

 private:
  scoped_ptr<ViewTreeHostImpl> host_;
  ViewTreeImpl* tree_;
  ConnectionManager* connection_manager_;
  bool connection_closed_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeHostConnection);
};

// Live implementation of ViewTreeHostConnection.
class ViewTreeHostConnectionImpl : public ViewTreeHostConnection {
 public:
  ViewTreeHostConnectionImpl(mojo::InterfaceRequest<mojo::ViewTreeHost> request,
                             scoped_ptr<ViewTreeHostImpl> host_impl,
                             mojo::ViewTreeClientPtr client,
                             ConnectionManager* connection_manager);

 private:
  ~ViewTreeHostConnectionImpl() override;

  // ViewTreeHostDelegate:
  void OnDisplayInitialized() override;

  mojo::Binding<mojo::ViewTreeHost> binding_;
  mojo::ViewTreeClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeHostConnectionImpl);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_VIEW_TREE_HOST_CONNECTION_H_
