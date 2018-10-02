// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/ash_window_manager.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
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
    DVLOG(1) << "AddWindowToTableMode passed invalid window, id=" << window_id;
}

void AshWindowManager::ShowSnapPreview(ws::Id window_id,
                                       mojom::SnapDirection snap) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window) &&
      caption_controller_.CanSnap(window)) {
    caption_controller_.ShowSnapPreview(window, snap);
  } else {
    DVLOG(1) << "ShowSnapPreview passed invalid window, id=" << window_id;
  }
}

void AshWindowManager::CommitSnap(ws::Id window_id, mojom::SnapDirection snap) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window) &&
      caption_controller_.CanSnap(window)) {
    caption_controller_.CommitSnap(window, snap);
  } else {
    DVLOG(1) << "CommitSnap passed invalid window, id=" << window_id;
  }
}

void AshWindowManager::MaximizeWindowByCaptionClick(
    ws::Id window_id,
    ui::mojom::PointerKind pointer) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (!window || !window_tree_->IsTopLevel(window)) {
    DVLOG(1) << "MaximizeWindowByCaptionClick passed invalid window, id="
             << window_id;
    return;
  }

  if (pointer == ui::mojom::PointerKind::MOUSE) {
    base::RecordAction(base::UserMetricsAction("Caption_ClickTogglesMaximize"));
  } else if (pointer == ui::mojom::PointerKind::TOUCH) {
    base::RecordAction(
        base::UserMetricsAction("Caption_GestureTogglesMaximize"));
  } else {
    DVLOG(1) << "MaximizeWindowByCaptionClick passed invalid event";
    return;
  }

  const wm::WMEvent wm_event(wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
  wm::GetWindowState(window)->OnWMEvent(&wm_event);
}

}  // namespace ash
