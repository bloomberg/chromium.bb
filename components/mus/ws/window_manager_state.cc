// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_state.h"

#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/server_window.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"

namespace mus {
namespace ws {

WindowManagerState::WindowManagerState(Display* display)
    : WindowManagerState(display, false, mojo::shell::mojom::kRootUserID) {}

WindowManagerState::WindowManagerState(Display* display, const UserId& user_id)
    : WindowManagerState(display, true, user_id) {}

WindowManagerState::~WindowManagerState() {}

WindowManagerState::WindowManagerState(Display* display,
                                       bool is_user_id_valid,
                                       const UserId& user_id)
    : display_(display),
      is_user_id_valid_(is_user_id_valid),
      user_id_(user_id) {
  ConnectionManager* connection_manager = display_->connection_manager();
  root_.reset(connection_manager->CreateServerWindow(
      connection_manager->display_manager()->GetAndAdvanceNextRootId(),
      ServerWindow::Properties()));
  // Our root is always a child of the Display's root. Do this
  // before the WindowTree has been created so that the client doesn't get
  // notified of the add, bounds change and visibility change.
  root_->SetBounds(gfx::Rect(display->root_window()->bounds().size()));
  root_->SetVisible(true);
  display->root_window()->Add(root_.get());
}

}  // namespace ws
}  // namespace mus
