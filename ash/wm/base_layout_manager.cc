// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_layout_manager.h"

#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, public:

BaseLayoutManager::BaseLayoutManager() {
  Shell::GetRootWindow()->AddRootWindowObserver(this);
}

BaseLayoutManager::~BaseLayoutManager() {
  for (WindowSet::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
  Shell::GetRootWindow()->RemoveRootWindowObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, LayoutManager overrides:

void BaseLayoutManager::OnWindowResized() {
}

void BaseLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  windows_.insert(child);
  child->AddObserver(this);
  if (child->GetProperty(aura::client::kShowStateKey))
    UpdateBoundsFromShowState(child);
}

void BaseLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
}

void BaseLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visibile) {
}

void BaseLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  // Avoid a janky resize on startup by ensuring the initial bounds fill the
  // screen.
  if (wm::IsWindowMaximized(child))
    child_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(child);
  else if (wm::IsWindowFullscreen(child))
    child_bounds = gfx::Screen::GetMonitorAreaNearestWindow(child);
  SetChildBoundsDirect(child, child_bounds);
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, RootWindowObserver overrides:

void BaseLayoutManager::OnRootWindowResized(const gfx::Size& new_size) {
  AdjustWindowSizesForScreenChange();
}

void BaseLayoutManager::OnScreenWorkAreaInsetsChanged() {
  AdjustWindowSizesForScreenChange();
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, WindowObserver overrides:

void BaseLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
  if (key == aura::client::kShowStateKey)
    UpdateBoundsFromShowState(window);
}

//////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, private:

void BaseLayoutManager::UpdateBoundsFromShowState(aura::Window* window) {
  switch (window->GetProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBounds(window);
      if (restore)
        window->SetBounds(*restore);
      window->ClearProperty(aura::client::kRestoreBoundsKey);
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

void BaseLayoutManager::AdjustWindowSizesForScreenChange() {
  // If a user plugs an external monitor into a laptop running Aura the
  // monitor size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (WindowSet::const_iterator it = windows_.begin();
       it != windows_.end();
       ++it) {
    aura::Window* window = *it;
    // The work area may be smaller than the full screen.
    gfx::Rect monitor_rect = wm::IsWindowFullscreen(window) ?
        gfx::Screen::GetMonitorAreaNearestWindow(window) :
        gfx::Screen::GetMonitorWorkAreaNearestWindow(window);
    // Put as much of the window as possible within the monitor area.
    window->SetBounds(window->bounds().AdjustToFit(monitor_rect));
  }
}

}  // namespace internal
}  // namespace ash
