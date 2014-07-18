// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/status_area_layout_manager.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "base/auto_reset.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, public:

StatusAreaLayoutManager::StatusAreaLayoutManager(aura::Window* container,
                                                 ShelfWidget* shelf)
    : SnapToPixelLayoutManager(container), in_layout_(false), shelf_(shelf) {
}

StatusAreaLayoutManager::~StatusAreaLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, aura::LayoutManager implementation:

void StatusAreaLayoutManager::OnWindowResized() {
  LayoutStatusArea();
}

void StatusAreaLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Only need to have the shelf do a layout if the child changing is the status
  // area and the shelf isn't in the process of doing a layout.
  if (child != shelf_->status_area_widget()->GetNativeView() || in_layout_) {
    SnapToPixelLayoutManager::SetChildBounds(child, requested_bounds);
    return;
  }

  // If the bounds match, no need to do anything. Check for target bounds to
  // ensure any active animation is retargeted.
  if (requested_bounds == child->GetTargetBounds())
    return;

  SnapToPixelLayoutManager::SetChildBounds(child, requested_bounds);
  LayoutStatusArea();
}

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, private:

void StatusAreaLayoutManager::LayoutStatusArea() {
  // Shelf layout manager may be already doing layout.
  if (shelf_->shelf_layout_manager()->updating_bounds())
    return;

  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  shelf_->shelf_layout_manager()->LayoutShelf();
}

}  // namespace ash
