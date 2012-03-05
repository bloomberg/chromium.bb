// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/shell.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

DECLARE_WINDOW_PROPERTY_TYPE(int)

namespace ash {
namespace internal {

namespace {

const aura::WindowProperty<int> kHeightBeforeObscuredProp = {0};
const aura::WindowProperty<int>* const kHeightBeforeObscuredKey =
    &kHeightBeforeObscuredProp;

void SetHeightBeforeObscured(aura::Window* window, int height) {
  window->SetProperty(kHeightBeforeObscuredKey, height);
}

int GetHeightBeforeObscured(aura::Window* window) {
  return window->GetProperty(kHeightBeforeObscuredKey);
}

void ClearHeightBeforeObscured(aura::Window* window) {
  window->SetProperty(kHeightBeforeObscuredKey, 0);
}

}  // namespace

WorkspaceWindowResizer::~WorkspaceWindowResizer() {
  if (root_filter_)
    root_filter_->UnlockCursor();
}

// static
WorkspaceWindowResizer* WorkspaceWindowResizer::Create(
    aura::Window* window,
    const gfx::Point& location,
    int window_component,
    int grid_size) {
  Details details(window, location, window_component, grid_size);
  return details.is_resizable ?
      new WorkspaceWindowResizer(details) : NULL;
}

void WorkspaceWindowResizer::Drag(const gfx::Point& location) {
  gfx::Rect bounds = CalculateBoundsForDrag(details_, location);
  if (constrain_size_)
    AdjustBoundsForMainWindow(&bounds);
  if (bounds != details_.window->bounds()) {
    did_move_or_resize_ = true;
    details_.window->SetBounds(bounds);
  }
}

void WorkspaceWindowResizer::CompleteDrag() {
  if (details_.grid_size <= 1 || !did_move_or_resize_ ||
      details_.window_component != HTCAPTION)
    return;

  gfx::Rect bounds(AdjustBoundsToGrid(details_));
  if (GetHeightBeforeObscured(window()) || constrain_size_) {
    // Two things can happen:
    // . We'll snap to the grid, which may result in different bounds. When
    //   dragging we only snap on release.
    // . If the bounds are different, and the windows height was truncated
    //   because it touched the bottom, than snapping to the grid may cause the
    //   window to no longer touch the bottom. Push it back up.
    gfx::Rect initial_bounds(window()->bounds());
    bool at_bottom = TouchesBottomOfScreen();
    if (at_bottom && bounds.y() != initial_bounds.y()) {
      if (bounds.y() < initial_bounds.y()) {
        bounds.set_height(bounds.height() + details_.grid_size -
                              (initial_bounds.y() - bounds.y()));
      }
      AdjustBoundsForMainWindow(&bounds);
    }
  }

  if (bounds == details_.window->bounds())
    return;

  if (bounds.size() != details_.window->bounds().size()) {
    // Don't attempt to animate a size change.
    details_.window->SetBounds(bounds);
    return;
  }

  ui::ScopedLayerAnimationSettings scoped_setter(
      details_.window->layer()->GetAnimator());
  // Use a small duration since the grid is small.
  scoped_setter.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
  details_.window->SetBounds(bounds);
}

void WorkspaceWindowResizer::RevertDrag() {
  if (!did_move_or_resize_)
    return;

  details_.window->SetBounds(details_.initial_bounds);
}

WorkspaceWindowResizer::WorkspaceWindowResizer(
    const Details& details)
    : details_(details),
      constrain_size_(wm::IsWindowNormal(details.window)),
      did_move_or_resize_(false),
      root_filter_(NULL) {
  DCHECK(details_.is_resizable);
  root_filter_ = Shell::GetInstance()->root_filter();
  if (root_filter_)
    root_filter_->LockCursor();

  if (is_resizable() && constrain_size_ &&
      (!TouchesBottomOfScreen() ||
       details_.bounds_change != kBoundsChange_Repositions)) {
    ClearCachedHeights();
  }
}

void WorkspaceWindowResizer::AdjustBoundsForMainWindow(
    gfx::Rect* bounds) const {
  gfx::Rect work_area(gfx::Screen::GetMonitorWorkAreaNearestWindow(window()));
  AdjustBoundsForWindow(work_area, window(), bounds);
}

void WorkspaceWindowResizer::AdjustBoundsForWindow(
    const gfx::Rect& work_area,
    aura::Window* window,
    gfx::Rect* bounds) const {
  if (bounds->bottom() < work_area.bottom()) {
    int height = GetHeightBeforeObscured(window);
    if (!height)
      return;
    height = std::max(bounds->height(), height);
    bounds->set_height(std::min(work_area.bottom() - bounds->y(), height));
    return;
  }

  if (bounds->bottom() == work_area.bottom())
    return;

  if (!GetHeightBeforeObscured(window))
    SetHeightBeforeObscured(window, window->bounds().height());

  gfx::Size min_size = window->delegate()->GetMinimumSize();
  bounds->set_height(
      std::max(0, work_area.bottom() - bounds->y()));
  if (bounds->height() < min_size.height()) {
    bounds->set_height(min_size.height());
    bounds->set_y(work_area.bottom() - min_size.height());
  }
}

void WorkspaceWindowResizer::ClearCachedHeights() {
  ClearHeightBeforeObscured(details_.window);
}

bool WorkspaceWindowResizer::TouchesBottomOfScreen() const {
  gfx::Rect work_area(
      gfx::Screen::GetMonitorWorkAreaNearestWindow(details_.window));
  return details_.window->bounds().bottom() == work_area.bottom();
}

}  // namespace internal
}  // namespace ash
