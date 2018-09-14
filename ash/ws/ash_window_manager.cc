// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/ash_window_manager.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/logging.h"
#include "services/ws/window_tree.h"

namespace ash {

AshWindowManager::AshWindowManager(ws::WindowTree* window_tree,
                                   mojo::ScopedInterfaceEndpointHandle handle)
    : window_tree_(window_tree),
      binding_(this,
               mojo::AssociatedInterfaceRequest<mojom::AshWindowManager>(
                   std::move(handle))) {}

AshWindowManager::~AshWindowManager() = default;

void AshWindowManager::AddWindowToTabletMode(ws::Id window_id) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window))
    Shell::Get()->tablet_mode_controller()->AddWindow(window);
  else
    DVLOG(1) << "AddWindowToTableMode passed in valid window, id=" << window_id;
}

}  // namespace ash
