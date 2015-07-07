// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_manager_root_connection.h"

#include "components/view_manager/connection_manager.h"
#include "components/view_manager/view_manager_root_impl.h"

namespace view_manager {

ViewManagerRootConnection::ViewManagerRootConnection(
    scoped_ptr<ViewManagerRootImpl> view_manager_root,
    ConnectionManager* manager)
    : root_(view_manager_root.Pass()),
      service_(nullptr),
      connection_manager_(manager),
      connection_closed_(false) {
  root_->Init(this);
}

ViewManagerRootConnection::~ViewManagerRootConnection() {
  // If this DCHECK fails then something has tried to delete this object without
  // calling CloseConnection.
  DCHECK(connection_closed_);
}


void ViewManagerRootConnection::CloseConnection() {
  // A connection error will trigger the display to close and so we want to make
  // sure we signal the ConnectionManager only once.
  if (connection_closed_)
    return;
  connection_manager()->OnRootConnectionClosed(this);
  connection_closed_ = true;
  delete this;
}

ViewManagerServiceImpl* ViewManagerRootConnection::GetViewManagerService() {
  return service_;
}

void ViewManagerRootConnection::OnDisplayClosed() {
  CloseConnection();
}

ViewManagerRootConnectionImpl::ViewManagerRootConnectionImpl(
    mojo::InterfaceRequest<mojo::ViewManagerRoot> request,
    scoped_ptr<ViewManagerRootImpl> root,
    mojo::ViewManagerClientPtr client,
    ConnectionManager* manager)
    : ViewManagerRootConnection(root.Pass(), manager),
      binding_(view_manager_root(), request.Pass()) {
  binding_.set_error_handler(this);

  connection_manager()->AddRoot(this);
  set_view_manager_service(connection_manager()->EmbedAtView(
      kInvalidConnectionId,
      view_manager_root()->root_view()->id(),
      client.Pass()));
}

ViewManagerRootConnectionImpl::~ViewManagerRootConnectionImpl() {
}

void ViewManagerRootConnectionImpl::OnConnectionError() {
  CloseConnection();
}

}  // namespace view_manager
