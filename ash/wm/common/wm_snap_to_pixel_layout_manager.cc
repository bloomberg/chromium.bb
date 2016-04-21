// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/wm_snap_to_pixel_layout_manager.h"

#include "ash/wm/common/wm_window.h"

namespace ash {
namespace wm {

WmSnapToPixelLayoutManager::WmSnapToPixelLayoutManager() {}

WmSnapToPixelLayoutManager::~WmSnapToPixelLayoutManager() {}

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
