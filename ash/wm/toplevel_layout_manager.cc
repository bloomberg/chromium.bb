// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_layout_manager.h"

#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

namespace {

// Minimum number of pixels at the window top to keep visible on screen.
const int kMinimumWindowTopVisible = 12;

// Returns a bounds rectangle with at least a few pixels of the window's
// title bar visible.
gfx::Rect BoundsWithTitleBarVisible(const gfx::Rect& window_bounds,
                                    const gfx::Rect& work_area) {
  gfx::Rect bounds(window_bounds);
  // Ensure title bar is vertically on screen.
  if (bounds.y() < work_area.y())
    bounds.set_y(work_area.y());
  else if (bounds.y() + kMinimumWindowTopVisible > work_area.bottom())
    bounds.set_y(work_area.bottom() - kMinimumWindowTopVisible);
  return bounds;
}

}  // namespace

namespace ash {
namespace internal {

/////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, public:

ToplevelLayoutManager::ToplevelLayoutManager()
    : shelf_(NULL) {
}

ToplevelLayoutManager::~ToplevelLayoutManager() {
}

/////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, LayoutManager overrides:

void ToplevelLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  BaseLayoutManager::OnWindowAddedToLayout(child);
  UpdateShelfVisibility();
}

void ToplevelLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
  UpdateShelfVisibility();
}

void ToplevelLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                           bool visible) {
  BaseLayoutManager::OnChildWindowVisibilityChanged(child, visible);
  if (child->type() == aura::client::WINDOW_TYPE_NORMAL ||
      child->type() == aura::client::WINDOW_TYPE_POPUP) {
    if (visible) {
      AnimateShowWindow(child);
    } else {
      // Don't start hiding the window again if it's already being hidden.
      if (child->layer()->GetTargetOpacity() != 0.0f)
        AnimateHideWindow(child);
    }
  }
  UpdateShelfVisibility();
}

void ToplevelLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  // Ensure normal windows have the title bar at least partly visible.
  if (!window_util::IsWindowMaximized(child) &&
      !window_util::IsWindowFullscreen(child)) {
    child_bounds = BoundsWithTitleBarVisible(
        child_bounds, gfx::Screen::GetMonitorWorkAreaNearestWindow(child));
  }
  BaseLayoutManager::SetChildBounds(child, child_bounds);
}

/////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, WindowObserver overrides:

void ToplevelLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                    const char* name,
                                                    void* old) {
  BaseLayoutManager::OnWindowPropertyChanged(window, name, old);
  if (name == aura::client::kShowStateKey)
    UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, private:

void ToplevelLayoutManager::UpdateShelfVisibility() {
  if (!shelf_)
    return;
  shelf_->SetVisible(!window_util::HasFullscreenWindow(windows()));
}

}  // namespace internal
}  // namespace ash
