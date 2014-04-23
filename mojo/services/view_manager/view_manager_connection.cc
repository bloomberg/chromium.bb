// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_connection.h"

#include "base/stl_util.h"
#include "mojo/services/view_manager/root_view_manager.h"
#include "mojo/services/view_manager/view.h"

namespace mojo {
namespace services {
namespace view_manager {

ViewManagerConnection::ViewManagerConnection() : id_(0) {
}

ViewManagerConnection::~ViewManagerConnection() {
  STLDeleteContainerPairSecondPointers(view_map_.begin(), view_map_.end());
  context()->RemoveConnection(this);
}

void ViewManagerConnection::Initialize(
    ServiceConnector<ViewManagerConnection, RootViewManager>* service_factory,
    ScopedMessagePipeHandle client_handle) {
  DCHECK_EQ(0, id_);  // Should only get Initialize() once.
  ServiceConnection<ViewManager, ViewManagerConnection, RootViewManager>::
      Initialize(service_factory, client_handle.Pass());
  context()->AddConnection(this);
  id_ = context()->GetAndAdvanceNextConnectionId();
  client()->OnConnectionEstablished(id_);
}

View* ViewManagerConnection::GetView(int32_t id) {
  ViewMap::iterator i = view_map_.find(id);
  return i == view_map_.end() ? NULL : i->second;
}

View* ViewManagerConnection::GetViewById(const ViewId& view_id) {
  const int32_t manager_id =
      view_id.manager_id() == 0 ? id_ : view_id.manager_id();
  return context()->GetView(manager_id, view_id.view_id());
}

void ViewManagerConnection::CreateView(
    int32_t view_id,
    const mojo::Callback<void(bool)>& callback) {
  // Negative values are reserved.
  if (view_map_.find(view_id) != view_map_.end() || view_id < 0) {
    callback.Run(false);
    return;
  }
  view_map_[view_id] = new View(view_id);
  callback.Run(true);
}

void ViewManagerConnection::AddView(
    const ViewId& parent_id,
    const ViewId& child_id,
    const mojo::Callback<void(bool)>& callback) {
  View* parent_view = GetViewById(parent_id);
  View* child_view = GetViewById(child_id);
  bool success = false;
  if (parent_view && child_view && parent_view != child_view) {
    parent_view->Add(child_view);
    success = true;
  }
  callback.Run(success);
}

void ViewManagerConnection::RemoveFromParent(
      const ViewId& view_id,
      const mojo::Callback<void(bool)>& callback) {
  View* view = GetViewById(view_id);
  bool success = false;
  if (view && view->GetParent()) {
    view->GetParent()->Add(view);
    success = true;
  }
  callback.Run(success);
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
