// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/wm_snap_to_pixel_layout_manager.h"

#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/memory/ptr_util.h"

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
    if (child->GetBoolProperty(
            WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY)) {
      if (!child->GetLayoutManager())
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
