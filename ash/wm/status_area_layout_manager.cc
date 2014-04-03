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

StatusAreaLayoutManager::StatusAreaLayoutManager(ShelfWidget* shelf)
    : in_layout_(false),
      shelf_(shelf) {
}

StatusAreaLayoutManager::~StatusAreaLayoutManager() {
}

////////////////////////////////////////////////////////////////////////////////
// StatusAreaLayoutManager, aura::LayoutManager implementation:

void StatusAreaLayoutManager::OnWindowResized() {
  LayoutStatusArea();
}

void StatusAreaLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void StatusAreaLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void StatusAreaLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}

void StatusAreaLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child, bool visible) {
}

void StatusAreaLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Only need to have the shelf do a layout if the child changing is the status
  // area and the shelf isn't in the process of doing a layout.
  if (child != shelf_->status_area_widget()->GetNativeView() || in_layout_) {
    SetChildBoundsDirect(child, requested_bounds);
    return;
  }

  // If the size matches, no need to do anything. We don't check the location as
  // that is managed by the shelf.
  if (requested_bounds == child->bounds())
    return;

  SetChildBoundsDirect(child, requested_bounds);
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
