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
      connection_manager_(manager) {
  root_->Init(this);
}

ViewManagerRootConnection::~ViewManagerRootConnection() {
}


ViewManagerServiceImpl* ViewManagerRootConnection::GetViewManagerService() {
  return service_;
}

void ViewManagerRootConnection::OnDisplayClosed() {
  connection_manager()->OnRootConnectionClosed(this);
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
  connection_manager()->OnRootConnectionClosed(this);
}

}  // namespace view_manager
