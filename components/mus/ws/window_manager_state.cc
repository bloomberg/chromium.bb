// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_state.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/server_window.h"

namespace mus {
namespace ws {

WindowManagerState::WindowManagerState(WindowTreeHostImpl* tree_host)
    : WindowManagerState(tree_host, false, 0u) {}

WindowManagerState::WindowManagerState(WindowTreeHostImpl* tree_host,
                                       uint32_t user_id)
    : WindowManagerState(tree_host, true, user_id) {}

WindowManagerState::~WindowManagerState() {}

WindowManagerState::WindowManagerState(WindowTreeHostImpl* tree_host,
                                       bool is_user_id_valid,
                                       uint32_t user_id)
    : tree_host_(tree_host),
      is_user_id_valid_(is_user_id_valid),
      user_id_(user_id) {
  ConnectionManager* connection_manager = tree_host_->connection_manager();
  root_.reset(connection_manager->CreateServerWindow(
      RootWindowId(connection_manager->GetAndAdvanceNextHostId()),
      ServerWindow::Properties()));
  // Our root is always a child of the WindowTreeHostImpl's root. Do this
  // before the WindowTree has been created so that the client doesn't get
  // notified of the add, bounds change and visibility change.
  root_->SetBounds(gfx::Rect(tree_host->root_window()->bounds().size()));
  root_->SetVisible(true);
  tree_host->root_window()->Add(root_.get());
}

}  // namespace ws
}  // namespace mus
