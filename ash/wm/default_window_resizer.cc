// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/root_window_event_filter.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

namespace ash {

DefaultWindowResizer::~DefaultWindowResizer() {
  if (root_filter_)
    root_filter_->UnlockCursor();
}

// static
DefaultWindowResizer*
DefaultWindowResizer::Create(aura::Window* window,
                             const gfx::Point& location,
                             int window_component) {
  Details details(window, location, window_component);
  return details.is_resizable ? new DefaultWindowResizer(details) : NULL;
}

void DefaultWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  int grid_size = event_flags & ui::EF_CONTROL_DOWN ?
                  0 : ash::Shell::GetInstance()->GetGridSize();
  gfx::Rect bounds(CalculateBoundsForDrag(details_, location, grid_size));
  if (bounds != details_.window->bounds()) {
    did_move_or_resize_ = true;
    details_.window->SetBounds(bounds);
  }
}

void DefaultWindowResizer::CompleteDrag(int event_flags) {
  int grid_size = event_flags & ui::EF_CONTROL_DOWN ?
                  0 : ash::Shell::GetInstance()->GetGridSize();
  if (grid_size <= 1 || !did_move_or_resize_)
    return;
  gfx::Rect new_bounds(
      AdjustBoundsToGrid(details_.window->bounds(), grid_size));
  if (new_bounds == details_.window->bounds())
    return;

  if (new_bounds.size() != details_.window->bounds().size()) {
    // Don't attempt to animate a size change.
    details_.window->SetBounds(new_bounds);
    return;
  }

  ui::ScopedLayerAnimationSettings scoped_setter(
      details_.window->layer()->GetAnimator());
  // Use a small duration since the grid is small.
  scoped_setter.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
  details_.window->SetBounds(new_bounds);
}

void DefaultWindowResizer::RevertDrag() {
  if (!did_move_or_resize_)
    return;

  details_.window->SetBounds(details_.initial_bounds);
}

DefaultWindowResizer::DefaultWindowResizer(const Details& details)
    : details_(details),
      did_move_or_resize_(false),
      root_filter_(NULL) {
  DCHECK(details_.is_resizable);
  root_filter_ = Shell::GetInstance()->root_filter();
  if (root_filter_)
    root_filter_->LockCursor();
}

}  // namespace aura
