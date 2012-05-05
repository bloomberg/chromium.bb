// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_layout_manager.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"

DECLARE_WINDOW_PROPERTY_TYPE(ui::WindowShowState)

namespace {

// Given a |window| and tentative |restore_bounds|, returns new bounds that
// ensure that at least a few pixels of the screen background are visible
// outside the edges of the window.  Use this to ensure that restoring a
// maximized window creates enough space that the resize handles are easily
// clickable.  We get into this state when updating Chrome OS R18 to R19, as
// Chrome OS R18 and earlier used only maximized windows and set their restore
// bounds to the size of the screen.  See crbug.com/108073
gfx::Rect BoundsWithScreenEdgeVisible(aura::Window* window,
                                      const gfx::Rect& restore_bounds) {
  // If the restore_bounds are more than 1 grid step away from the size the
  // window would be when maximized, inset it.
  int grid_size = ash::Shell::GetInstance()->GetGridSize();
  gfx::Rect max_bounds = ash::ScreenAsh::GetMaximizedWindowBounds(window);
  max_bounds.Inset(grid_size, grid_size);
  if (restore_bounds.Contains(max_bounds))
    return max_bounds;
  return restore_bounds;
}

// Used to remember the show state before the window was minimized.
DEFINE_WINDOW_PROPERTY_KEY(
    ui::WindowShowState, kRestoreShowStateKey, ui::SHOW_STATE_DEFAULT);

}  // namespace

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
  // Only update the bounds if the window has a show state that depends on the
  // workspace area.
  if (wm::IsWindowMaximized(child) || wm::IsWindowFullscreen(child))
    UpdateBoundsFromShowState(child);
}

void BaseLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
}

void BaseLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}

void BaseLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {
  if (visible && wm::IsWindowMinimized(child)) {
    // Attempting to show a minimized window. Unminimize it.
    child->SetProperty(aura::client::kShowStateKey,
                       child->GetProperty(kRestoreShowStateKey));
    child->ClearProperty(kRestoreShowStateKey);
  }
}

void BaseLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  // Avoid a janky resize on startup by ensuring the initial bounds fill the
  // screen.
  if (wm::IsWindowMaximized(child))
    child_bounds = ScreenAsh::GetMaximizedWindowBounds(child);
  else if (wm::IsWindowFullscreen(child))
    child_bounds = gfx::Screen::GetMonitorNearestWindow(child).bounds();
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
  if (key == aura::client::kShowStateKey) {
    UpdateBoundsFromShowState(window);
    ShowStateChanged(window, static_cast<ui::WindowShowState>(old));
  }
}

void BaseLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = NULL;
  }
}

//////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, private:

void BaseLayoutManager::ShowStateChanged(aura::Window* window,
                                         ui::WindowShowState last_show_state) {
  if (wm::IsWindowMinimized(window)) {
    // Save the previous show state so that we can correctly restore it.
    window->SetProperty(kRestoreShowStateKey, last_show_state);
    SetWindowVisibilityAnimationType(
        window, WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

    // Hide the window.
    window->Hide();
    // Activate another window.
    if (wm::IsActiveWindow(window))
      wm::DeactivateWindow(window);
  } else if ((window->TargetVisibility() ||
              last_show_state == ui::SHOW_STATE_MINIMIZED) &&
             !window->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window->Show();
  }
}

void BaseLayoutManager::UpdateBoundsFromShowState(aura::Window* window) {
  switch (window->GetProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBounds(window);
      if (restore) {
        SetChildBoundsDirect(window,
                             BoundsWithScreenEdgeVisible(window, *restore));
      }
      window->ClearProperty(aura::client::kRestoreBoundsKey);
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
      SetRestoreBoundsIfNotSet(window);
      SetChildBoundsDirect(window, ScreenAsh::GetMaximizedWindowBounds(window));
      break;

    case ui::SHOW_STATE_FULLSCREEN:
      SetRestoreBoundsIfNotSet(window);
      SetChildBoundsDirect(
          window, gfx::Screen::GetMonitorNearestWindow(window).bounds());
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
      SetChildBoundsDirect(
          window, gfx::Screen::GetMonitorNearestWindow(window).bounds());
    } else {
      // The work area may be smaller than the full screen.
      gfx::Rect monitor_rect =
          gfx::Screen::GetMonitorNearestWindow(window).work_area();
      // Put as much of the window as possible within the monitor area.
      window->SetBounds(window->bounds().AdjustToFit(monitor_rect));
    }
  }
}

}  // namespace internal
}  // namespace ash
