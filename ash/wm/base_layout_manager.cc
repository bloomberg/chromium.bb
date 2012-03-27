// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_layout_manager.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
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

BaseLayoutManager::BaseLayoutManager(aura::RootWindow* root_window)
    : root_window_(root_window) {
  Shell::GetInstance()->AddShellObserver(this);
  root_window_->AddRootWindowObserver(this);
  root_window_->AddObserver(this);
}

BaseLayoutManager::~BaseLayoutManager() {
  if (root_window_) {
    root_window_->RemoveObserver(this);
    root_window_->RemoveRootWindowObserver(this);
  }
  for (WindowSet::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
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
    child_bounds = ScreenAsh::GetMaximizedWindowBounds(child);
  else if (wm::IsWindowFullscreen(child))
    child_bounds = gfx::Screen::GetMonitorAreaNearestWindow(child);
  SetChildBoundsDirect(child, child_bounds);
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, RootWindowObserver overrides:

void BaseLayoutManager::OnRootWindowResized(const aura::RootWindow* root,
                                            const gfx::Size& old_size) {
  AdjustWindowSizesForScreenChange();
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, ash::ShellObserver overrides:

void BaseLayoutManager::OnMonitorWorkAreaInsetsChanged() {
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

void BaseLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = NULL;
  }
}

//////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, private:

void BaseLayoutManager::UpdateBoundsFromShowState(aura::Window* window) {
  switch (window->GetProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBounds(window);
      if (restore)
        SetChildBoundsDirect(window, *restore);
      window->ClearProperty(aura::client::kRestoreBoundsKey);
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
      SetRestoreBoundsIfNotSet(window);
      SetChildBoundsDirect(window, ScreenAsh::GetMaximizedWindowBounds(window));
      break;

    case ui::SHOW_STATE_FULLSCREEN:
      SetRestoreBoundsIfNotSet(window);
      SetChildBoundsDirect(window,
                           gfx::Screen::GetMonitorAreaNearestWindow(window));
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
    if (wm::IsWindowMaximized(window)) {
      SetChildBoundsDirect(window, ScreenAsh::GetMaximizedWindowBounds(window));
    } else if (wm::IsWindowFullscreen(window)) {
      SetChildBoundsDirect(window,
                           gfx::Screen::GetMonitorAreaNearestWindow(window));
    } else {
      // The work area may be smaller than the full screen.
      gfx::Rect monitor_rect =
          gfx::Screen::GetMonitorWorkAreaNearestWindow(window);
      // Put as much of the window as possible within the monitor area.
      window->SetBounds(window->bounds().AdjustToFit(monitor_rect));
    }
  }
}

}  // namespace internal
}  // namespace ash
