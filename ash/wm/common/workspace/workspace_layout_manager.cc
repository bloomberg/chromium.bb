// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/workspace/workspace_layout_manager.h"

#include <algorithm>

#include "ash/wm/common/always_on_top_controller.h"
#include "ash/wm/common/fullscreen_window_finder.h"
#include "ash/wm/common/window_positioner.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/common/wm_globals.h"
#include "ash/wm/common/wm_root_window_controller.h"
#include "ash/wm/common/wm_screen_util.h"
#include "ash/wm/common/wm_window.h"
#include "ash/wm/common/wm_window_property.h"
#include "ash/wm/common/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "ash/wm/common/workspace/workspace_layout_manager_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace ash {

WorkspaceLayoutManager::WorkspaceLayoutManager(
    wm::WmWindow* window,
    std::unique_ptr<wm::WorkspaceLayoutManagerDelegate> delegate)
    : window_(window),
      root_window_(window->GetRootWindow()),
      root_window_controller_(root_window_->GetRootWindowController()),
      globals_(window_->GetGlobals()),
      delegate_(std::move(delegate)),
      work_area_in_parent_(wm::GetDisplayWorkAreaBounds(window_)),
      is_fullscreen_(wm::GetWindowForFullscreenMode(window) != nullptr) {
  globals_->AddActivationObserver(this);
  root_window_->AddObserver(this);
  root_window_controller_->AddObserver(this);
  DCHECK(window->GetBoolProperty(
      wm::WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUDARY));
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  if (root_window_)
    root_window_->RemoveObserver(this);
  for (wm::WmWindow* window : windows_)
    window->RemoveObserver(this);
  root_window_->GetRootWindowController()->RemoveObserver(this);
  globals_->RemoveActivationObserver(this);
}

void WorkspaceLayoutManager::DeleteDelegate() {
  delegate_.reset();
}

void WorkspaceLayoutManager::SetMaximizeBackdropDelegate(
    std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate) {
  backdrop_delegate_.reset(delegate.release());
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowResized() {}

void WorkspaceLayoutManager::OnWindowAddedToLayout(wm::WmWindow* child) {
  wm::WindowState* window_state = child->GetWindowState();
  wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
  windows_.insert(child);
  child->AddObserver(this);
  window_state->AddObserver(this);
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowAddedToLayout(child);
  WindowPositioner::RearrangeVisibleWindowOnShow(child);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(wm::WmWindow* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  child->GetWindowState()->RemoveObserver(this);

  if (child->GetTargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(wm::WmWindow* child) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowRemovedFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(wm::WmWindow* child,
                                                            bool visible) {
  wm::WindowState* window_state = child->GetWindowState();
  // Attempting to show a minimized window. Unminimize it.
  if (visible && window_state->IsMinimized())
    window_state->Unminimize();

  if (child->GetTargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnShow(child);
  else
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
  UpdateFullscreenState();
  UpdateShelfVisibility();
  if (backdrop_delegate_)
    backdrop_delegate_->OnChildWindowVisibilityChanged(child, visible);
}

void WorkspaceLayoutManager::SetChildBounds(wm::WmWindow* child,
                                            const gfx::Rect& requested_bounds) {
  wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, requested_bounds);
  child->GetWindowState()->OnWMEvent(&event);
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, keyboard::KeyboardControllerObserver implementation:

void WorkspaceLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  wm::WmWindow* window = globals_->GetActiveWindow();
  if (!window)
    return;

  window = window->GetToplevelWindow();
  if (!window_->Contains(window))
    return;
  wm::WindowState* window_state = window->GetWindowState();
  if (!new_bounds.IsEmpty()) {
    // Store existing bounds to be restored before resizing for keyboard if it
    // is not already stored.
    if (!window_state->HasRestoreBounds())
      window_state->SaveCurrentBoundsForRestore();

    gfx::Rect window_bounds =
        window_->ConvertRectToScreen(window->GetTargetBounds());
    int vertical_displacement =
        std::max(0, window_bounds.bottom() - new_bounds.y());
    int shift = std::min(vertical_displacement,
                         window_bounds.y() - work_area_in_parent_.y());
    if (shift > 0) {
      gfx::Point origin(window_bounds.x(), window_bounds.y() - shift);
      SetChildBounds(window, gfx::Rect(origin, window_bounds.size()));
    }
  } else if (window_state->HasRestoreBounds()) {
    // Keyboard hidden, restore original bounds if they exist. If the user has
    // resized or dragged the window in the meantime, WorkspaceWindowResizer
    // will have cleared the restore bounds and this code will not accidentally
    // override user intent.
    window_state->SetAndClearRestoreBounds();
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, wm::WmRootWindowControllerObserver implementation:

void WorkspaceLayoutManager::OnWorkAreaChanged() {
  const gfx::Rect work_area(wm::GetDisplayWorkAreaBounds(window_));
  if (work_area != work_area_in_parent_) {
    const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&event);
  }
  if (backdrop_delegate_)
    backdrop_delegate_->OnDisplayWorkAreaInsetsChanged();
}

void WorkspaceLayoutManager::OnFullscreenStateChanged(bool is_fullscreen) {
  if (is_fullscreen_ == is_fullscreen)
    return;

  is_fullscreen_ = is_fullscreen;
  wm::WmWindow* fullscreen_window =
      is_fullscreen ? GetWindowForFullscreenMode(window_) : nullptr;
  // Changing always on top state may change window's parent. Iterate on a copy
  // of |windows_| to avoid invalidating an iterator. Since both workspace and
  // always_on_top containers' layouts are managed by this class all the
  // appropriate windows will be included in the iteration.
  WindowSet windows(windows_);
  for (auto window : windows) {
    wm::WindowState* window_state = window->GetWindowState();
    if (is_fullscreen)
      window_state->DisableAlwaysOnTop(fullscreen_window);
    else
      window_state->RestoreAlwaysOnTop();
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::WindowObserver implementation:

void WorkspaceLayoutManager::OnWindowTreeChanged(
    wm::WmWindow* window,
    const wm::WmWindowObserver::TreeChangeParams& params) {
  if (!params.target->GetWindowState()->IsActive())
    return;
  // If the window is already tracked by the workspace this update would be
  // redundant as the fullscreen and shelf state would have been handled in
  // OnWindowAddedToLayout.
  if (windows_.find(params.target) != windows_.end())
    return;

  // If the active window has moved to this root window then update the
  // fullscreen state.
  // TODO(flackr): Track the active window leaving this root window and update
  // the fullscreen state accordingly.
  if (params.new_parent && params.new_parent->GetRootWindow() == root_window_) {
    UpdateFullscreenState();
    UpdateShelfVisibility();
  }
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(
    wm::WmWindow* window,
    wm::WmWindowProperty property,
    intptr_t old) {
  if (property == wm::WmWindowProperty::ALWAYS_ON_TOP &&
      window->GetBoolProperty(wm::WmWindowProperty::ALWAYS_ON_TOP)) {
    root_window_controller_->GetAlwaysOnTopController()
        ->GetContainer(window)
        ->AddChild(window);
  }
}

void WorkspaceLayoutManager::OnWindowStackingChanged(wm::WmWindow* window) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowStackingChanged(window);
}

void WorkspaceLayoutManager::OnWindowDestroying(wm::WmWindow* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
  }
}

void WorkspaceLayoutManager::OnWindowBoundsChanged(
    wm::WmWindow* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (root_window_ == window) {
    const wm::WMEvent wm_event(wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&wm_event);
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager,
// aura::client::ActivationChangeObserver implementation:

void WorkspaceLayoutManager::OnWindowActivated(wm::WmWindow* gained_active,
                                               wm::WmWindow* lost_active) {
  wm::WindowState* window_state =
      gained_active ? gained_active->GetWindowState() : nullptr;
  if (window_state && window_state->IsMinimized() &&
      !gained_active->IsVisible()) {
    window_state->Unminimize();
    DCHECK(!window_state->IsMinimized());
  }
  UpdateFullscreenState();
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, wm::WindowStateObserver implementation:

void WorkspaceLayoutManager::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    wm::WindowStateType old_type) {
  // Notify observers that fullscreen state may be changing.
  if (window_state->IsFullscreen() ||
      old_type == wm::WINDOW_STATE_TYPE_FULLSCREEN) {
    UpdateFullscreenState();
  }

  UpdateShelfVisibility();
  if (backdrop_delegate_)
    backdrop_delegate_->OnPostWindowStateTypeChange(window_state, old_type);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, private:

void WorkspaceLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    const wm::WMEvent* event) {
  DCHECK(event->type() == wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED ||
         event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);

  work_area_in_parent_ = wm::GetDisplayWorkAreaBounds(window_);

  // Don't do any adjustments of the insets while we are in screen locked mode.
  // This would happen if the launcher was auto hidden before the login screen
  // was shown and then gets shown when the login screen gets presented.
  if (event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED &&
      globals_->IsScreenLocked())
    return;

  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (wm::WmWindow* window : windows_)
    window->GetWindowState()->OnWMEvent(event);
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  if (delegate_)
    delegate_->UpdateShelfVisibility();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  // TODO(flackr): The fullscreen state is currently tracked per workspace
  // but the shell notification implies a per root window state. Currently
  // only windows in the default workspace container will go fullscreen but
  // this should really be tracked by the RootWindowController since
  // technically any container could get a fullscreen window.
  if (!delegate_)
    return;
  bool is_fullscreen = GetWindowForFullscreenMode(window_) != nullptr;
  if (is_fullscreen != is_fullscreen_) {
    delegate_->OnFullscreenStateChanged(is_fullscreen);
    is_fullscreen_ = is_fullscreen;
  }
}

}  // namespace ash
