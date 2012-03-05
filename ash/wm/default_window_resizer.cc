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
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
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
                             int window_component,
                             int grid_size) {
  Details details(window, location, window_component, grid_size);
  return details.is_resizable ? new DefaultWindowResizer(details) : NULL;
}

void DefaultWindowResizer::Drag(const gfx::Point& location) {
  gfx::Rect bounds(CalculateBoundsForDrag(details_, location));
  if (bounds != details_.window->bounds()) {
    did_move_or_resize_ = true;
    details_.window->SetBounds(bounds);
  }
}

void DefaultWindowResizer::CompleteDrag() {
  if (details_.grid_size <= 1 || !did_move_or_resize_)
    return;
  gfx::Rect new_bounds(AdjustBoundsToGrid(details_));
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
