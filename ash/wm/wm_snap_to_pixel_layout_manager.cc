// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_snap_to_pixel_layout_manager.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/window_properties.h"
#include "ash/wm_window.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {

WmSnapToPixelLayoutManager::WmSnapToPixelLayoutManager() {}

WmSnapToPixelLayoutManager::~WmSnapToPixelLayoutManager() {}

// static
void WmSnapToPixelLayoutManager::InstallOnContainers(WmWindow* window) {
  for (WmWindow* child : window->GetChildren()) {
    if (child->GetShellWindowId() < kShellWindowId_Min ||
        child->GetShellWindowId() > kShellWindowId_Max)  // not a container
      continue;
    if (child->aura_window()->GetProperty(kSnapChildrenToPixelBoundary)) {
      if (!child->GetLayoutManager() && !child->aura_window()->layout_manager())
        child->SetLayoutManager(base::MakeUnique<WmSnapToPixelLayoutManager>());
    } else {
      InstallOnContainers(child);
    }
  }
}

void WmSnapToPixelLayoutManager::OnWindowResized() {}

void WmSnapToPixelLayoutManager::OnWindowAddedToLayout(WmWindow* child) {}

void WmSnapToPixelLayoutManager::OnWillRemoveWindowFromLayout(WmWindow* child) {
}

void WmSnapToPixelLayoutManager::OnWindowRemovedFromLayout(WmWindow* child) {}

void WmSnapToPixelLayoutManager::OnChildWindowVisibilityChanged(WmWindow* child,
                                                                bool visibile) {
}

void WmSnapToPixelLayoutManager::SetChildBounds(
    WmWindow* child,
    const gfx::Rect& requested_bounds) {
  child->SetBoundsDirect(requested_bounds);
  child->SnapToPixelBoundaryIfNecessary();
}

}  // namespace wm
}  // namespace ash
