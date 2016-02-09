// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/snap_to_pixel_layout_manager.h"

#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {

SnapToPixelLayoutManager::SnapToPixelLayoutManager(aura::Window* container) {
}

SnapToPixelLayoutManager::~SnapToPixelLayoutManager() {
}

void SnapToPixelLayoutManager::OnWindowResized() {
}

void SnapToPixelLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void SnapToPixelLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void SnapToPixelLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}

void SnapToPixelLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visibile) {
}

void SnapToPixelLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
  wm::SnapWindowToPixelBoundary(child);
}

}  // namespace ash
