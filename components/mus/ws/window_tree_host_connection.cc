// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_host_connection.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "components/mus/ws/window_tree_impl.h"

namespace mus {

namespace ws {

WindowTreeHostConnection::WindowTreeHostConnection(
    scoped_ptr<WindowTreeHostImpl> host_impl,
    ConnectionManager* manager)
    : host_(std::move(host_impl)),
      tree_(nullptr),
      connection_manager_(manager),
      connection_closed_(false) {}

WindowTreeHostConnection::~WindowTreeHostConnection() {
  // If this DCHECK fails then something has tried to delete this object without
  // calling CloseConnection.
  DCHECK(connection_closed_);
}

void WindowTreeHostConnection::CloseConnection() {
  // A connection error will trigger the display to close and so we want to make
  // sure we signal the ConnectionManager only once.
  if (connection_closed_)
    return;
  // We have to shut down the focus system for this host before destroying the
  // host, as destruction of the window tree will attempt to change focus.
  host_->DestroyFocusController();
  connection_manager()->OnHostConnectionClosed(this);
  connection_closed_ = true;
  delete this;
}

const WindowTreeImpl* WindowTreeHostConnection::GetWindowTree() const {
  return tree_;
}

void WindowTreeHostConnection::OnDisplayInitialized() {}

void WindowTreeHostConnection::OnDisplayClosed() {
  CloseConnection();
}

WindowTreeHostConnectionImpl::WindowTreeHostConnectionImpl(
    mojo::InterfaceRequest<mojom::WindowTreeHost> request,
    scoped_ptr<WindowTreeHostImpl> host_impl,
    mojom::WindowTreeClientPtr client,
    ConnectionManager* manager)
    : WindowTreeHostConnection(std::move(host_impl), manager),
      binding_(window_tree_host(), std::move(request)),
      client_(std::move(client)) {}

WindowTreeHostConnectionImpl::~WindowTreeHostConnectionImpl() {}

void WindowTreeHostConnectionImpl::OnDisplayInitialized() {
  connection_manager()->AddHost(this);
  WindowTreeImpl* tree = connection_manager()->EmbedAtWindow(
      window_tree_host()->root_window(),
      mojom::WindowTree::kAccessPolicyEmbedRoot, std::move(client_));
  tree->ConfigureWindowManager();
  set_window_tree(tree);
}

}  // namespace ws

}  // namespace mus
