// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_targeter.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shelf_types.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

gfx::Insets GetInsetsForAlignment(int distance, ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_LEFT)
    return gfx::Insets(0, 0, 0, distance);
  if (alignment == SHELF_ALIGNMENT_RIGHT)
    return gfx::Insets(0, distance, 0, 0);
  return gfx::Insets(distance, 0, 0, 0);
}

}  // namespace

ShelfWindowTargeter::ShelfWindowTargeter(WmWindow* container, WmShelf* shelf)
    : ::wm::EasyResizeWindowTargeter(WmWindow::GetAuraWindow(container),
                                     gfx::Insets(),
                                     gfx::Insets()),
      shelf_(shelf) {
  WillChangeVisibilityState(shelf_->GetVisibilityState());
  container->aura_window()->AddObserver(this);
  shelf_->AddObserver(this);
}

ShelfWindowTargeter::~ShelfWindowTargeter() {
  // Ensure that the observers were removed and the shelf pointer was cleared.
  DCHECK(!shelf_);
}

void ShelfWindowTargeter::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  shelf_->RemoveObserver(this);
  shelf_ = nullptr;
}

void ShelfWindowTargeter::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  gfx::Insets mouse_insets;
  gfx::Insets touch_insets;
  if (new_state == SHELF_VISIBLE) {
    // Let clicks at the very top of the shelf through so windows can be
    // resized with the bottom-right corner and bottom edge.
    mouse_insets = GetInsetsForAlignment(kWorkspaceAreaVisibleInset,
                                         shelf_->GetAlignment());
  } else if (new_state == SHELF_AUTO_HIDE) {
    // Extend the touch hit target out a bit to allow users to drag shelf out
    // while hidden.
    touch_insets = GetInsetsForAlignment(-kWorkspaceAreaAutoHideInset,
                                         shelf_->GetAlignment());
  }

  set_mouse_extend(mouse_insets);
  set_touch_extend(touch_insets);
}

}  // namespace ash
