// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_layout_manager.h"

#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

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
    : shelf_(NULL),
      status_area_widget_(NULL) {
  aura::RootWindow::GetInstance()->AddRootWindowObserver(this);
}

ToplevelLayoutManager::~ToplevelLayoutManager() {
  for (WindowSet::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
  aura::RootWindow::GetInstance()->RemoveRootWindowObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, LayoutManager overrides:

void ToplevelLayoutManager::OnWindowResized() {
}

void ToplevelLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  if (child->GetProperty(aura::client::kShowStateKey)) {
    UpdateBoundsFromShowState(child);
    UpdateShelfVisibility();
    UpdateStatusAreaVisibility();
  }
}

void ToplevelLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  UpdateShelfVisibility();
  UpdateStatusAreaVisibility();
}

void ToplevelLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                           bool visible) {
  if (child->type() == aura::client::WINDOW_TYPE_NORMAL ||
      child->type() == aura::client::WINDOW_TYPE_POPUP) {
    visible ? AnimateShowWindow(child) : AnimateHideWindow(child);
  }
  UpdateShelfVisibility();
  UpdateStatusAreaVisibility();
}

void ToplevelLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  // Avoid a janky resize on startup by ensuring the initial bounds fill the
  // screen.
  if (window_util::IsWindowMaximized(child))
    child_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(child);
  else if (window_util::IsWindowFullscreen(child))
    child_bounds = gfx::Screen::GetMonitorAreaNearestWindow(child);
  else
    child_bounds = BoundsWithTitleBarVisible(
        child_bounds, gfx::Screen::GetMonitorWorkAreaNearestWindow(child));
  SetChildBoundsDirect(child, child_bounds);
}

/////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, RootWindowObserver overrides:

void ToplevelLayoutManager::OnRootWindowResized(const gfx::Size& new_size) {
  // If a user plugs an external monitor into a laptop running Aura the
  // monitor size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  for (WindowSet::const_iterator it = windows_.begin();
       it != windows_.end();
       ++it) {
    aura::Window* window = *it;
    // The work area may be smaller than the full screen.
    gfx::Rect monitor_rect = window_util::IsWindowFullscreen(window) ?
        gfx::Screen::GetMonitorAreaNearestWindow(window) :
        gfx::Screen::GetMonitorWorkAreaNearestWindow(window);
    // Put as much of the window as possible within the monitor area.
    window->SetBounds(window->bounds().AdjustToFit(monitor_rect));
  }
}

/////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, WindowObserver overrides:

void ToplevelLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                    const char* name,
                                                    void* old) {
  if (name == aura::client::kShowStateKey) {
    UpdateBoundsFromShowState(window);
    UpdateShelfVisibility();
    UpdateStatusAreaVisibility();
  }
}

//////////////////////////////////////////////////////////////////////////////
// ToplevelLayoutManager, private:

void ToplevelLayoutManager::UpdateBoundsFromShowState(aura::Window* window) {
  switch (window->GetIntProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBounds(window);
      window->SetProperty(aura::client::kRestoreBoundsKey, NULL);
      if (restore)
        window->SetBounds(*restore);
      delete restore;
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
      SetRestoreBoundsIfNotSet(window);
      window->SetBounds(gfx::Screen::GetMonitorWorkAreaNearestWindow(window));
      break;

    case ui::SHOW_STATE_FULLSCREEN:
      SetRestoreBoundsIfNotSet(window);
      window->SetBounds(gfx::Screen::GetMonitorAreaNearestWindow(window));
      break;

    default:
      break;
  }
}

void ToplevelLayoutManager::UpdateShelfVisibility() {
  if (!shelf_)
    return;
  shelf_->SetVisible(!window_util::HasFullscreenWindow(windows_));
}

void ToplevelLayoutManager::UpdateStatusAreaVisibility() {
  if (!status_area_widget_)
    return;
  // Full screen windows should hide the status area widget.
  if (window_util::HasFullscreenWindow(windows_))
    status_area_widget_->Hide();
  else
    status_area_widget_->Show();
}

}  // namespace internal
}  // namespace ash
