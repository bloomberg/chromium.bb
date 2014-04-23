// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_view_manager.h"

#include "base/logging.h"
#include "mojo/services/view_manager/view_manager_connection.h"

namespace mojo {
namespace services {
namespace view_manager {
namespace {

// Identifies the root of the view hierarchy.
const int32_t kRootId = -1;

}  // namespace

RootViewManager::ScopedChange::ScopedChange(ViewManagerConnection* connection,
                                            RootViewManager* root,
                                            int32_t change_id)
    : root_(root) {
  root_->PrepareForChange(connection, change_id);
}

RootViewManager::ScopedChange::~ScopedChange() {
  root_->FinishChange();
}

RootViewManager::RootViewManager()
    : next_connection_id_(1),
      root_(this, kRootId) {
}

RootViewManager::~RootViewManager() {
}

int32_t RootViewManager::GetAndAdvanceNextConnectionId() {
  return next_connection_id_++;
}

void RootViewManager::AddConnection(ViewManagerConnection* connection) {
  DCHECK_EQ(0u, connection_map_.count(connection->id()));
  connection_map_[connection->id()] = connection;
}

void RootViewManager::RemoveConnection(ViewManagerConnection* connection) {
  connection_map_.erase(connection->id());
}

View* RootViewManager::GetView(int32_t manager_id, int32_t view_id) {
  if (view_id == kRootId)
    return &root_;
  ConnectionMap::iterator i = connection_map_.find(manager_id);
  return i == connection_map_.end() ? NULL : i->second->GetView(view_id);
}

void RootViewManager::NotifyViewHierarchyChanged(const ViewId& view,
                                                 const ViewId& new_parent,
                                                 const ViewId& old_parent) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    const int32_t change_id = (change_ && i->first == change_->connection_id) ?
        change_->change_id : 0;
    i->second->NotifyViewHierarchyChanged(
        view, new_parent, old_parent, change_id);
  }
}

void RootViewManager::PrepareForChange(ViewManagerConnection* connection,
                                       int32_t change_id) {
  DCHECK(!change_.get());  // Should only ever have one change in flight.
  change_.reset(new Change(connection->id(), change_id));
}

void RootViewManager::FinishChange() {
  DCHECK(change_.get());  // PrepareForChange/FinishChange should be balanced.
  change_.reset();
}

void RootViewManager::OnCreated() {
}

void RootViewManager::OnDestroyed() {
}

void RootViewManager::OnBoundsChanged(const Rect& bounds) {
}

void RootViewManager::OnEvent(const Event& event,
                              const mojo::Callback<void()>& callback) {
  callback.Run();
}

ViewId RootViewManager::GetViewId(const View* view) const {
  ViewId::Builder builder;
  builder.set_manager_id(0);
  builder.set_view_id(view->id());
  return builder.Finish();
}

void RootViewManager::OnViewHierarchyChanged(const ViewId& view,
                                             const ViewId& new_parent,
                                             const ViewId& old_parent) {
  NotifyViewHierarchyChanged(view, new_parent, old_parent);
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
