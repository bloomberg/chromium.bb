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

RootViewManager::RootViewManager()
    : next_connection_id_(1),
      root_(kRootId) {
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

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
