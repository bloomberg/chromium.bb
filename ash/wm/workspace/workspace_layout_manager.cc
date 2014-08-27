// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <algorithm>

#include "ash/display/display_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_layout_manager_delegate.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

using aura::Window;

namespace ash {

WorkspaceLayoutManager::WorkspaceLayoutManager(aura::Window* window)
    : shelf_(NULL),
      window_(window),
      root_window_(window->GetRootWindow()),
      work_area_in_parent_(ScreenUtil::ConvertRectFromScreen(
          window_,
          Shell::GetScreen()->GetDisplayNearestWindow(window_).work_area())),
      is_fullscreen_(GetRootWindowController(
          window->GetRootWindow())->GetWindowForFullscreenMode() != NULL) {
  Shell::GetInstance()->activation_client()->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
  root_window_->AddObserver(this);
  DCHECK(window->GetProperty(kSnapChildrenToPixelBoundary));
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  if (root_window_)
    root_window_->RemoveObserver(this);
  for (WindowSet::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
}

void WorkspaceLayoutManager::SetShelf(ShelfLayoutManager* shelf) {
  shelf_ = shelf;
}

void WorkspaceLayoutManager::SetMaximizeBackdropDelegate(
    scoped_ptr<WorkspaceLayoutManagerDelegate> delegate) {
  backdrop_delegate_.reset(delegate.release());
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowAddedToLayout(Window* child) {
  wm::WindowState* window_state = wm::GetWindowState(child);
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

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  wm::GetWindowState(child)->RemoveObserver(this);

  if (child->TargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(Window* child) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowRemovedFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(Window* child,
                                                            bool visible) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  // Attempting to show a minimized window. Unminimize it.
  if (visible && window_state->IsMinimized())
    window_state->Unminimize();

  if (child->TargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnShow(child);
  else
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
  UpdateFullscreenState();
  UpdateShelfVisibility();
  if (backdrop_delegate_)
    backdrop_delegate_->OnChildWindowVisibilityChanged(child, visible);
}

void WorkspaceLayoutManager::SetChildBounds(
    Window* child,
    const gfx::Rect& requested_bounds) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, requested_bounds);
  window_state->OnWMEvent(&event);
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, keyboard::KeyboardControllerObserver implementation:

void WorkspaceLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  ui::InputMethod* input_method =
      root_window_->GetProperty(aura::client::kRootWindowInputMethodKey);
  ui::TextInputClient* text_input_client = input_method->GetTextInputClient();
  if (!text_input_client)
    return;
  aura::Window *window =
      text_input_client->GetAttachedWindow()->GetToplevelWindow();
  if (!window || !window_->Contains(window))
    return;
  wm::WindowState* window_state = wm::GetWindowState(window);
  if (!new_bounds.IsEmpty()) {
    // Store existing bounds to be restored before resizing for keyboard if it
    // is not already stored.
    if (!window_state->HasRestoreBounds())
      window_state->SaveCurrentBoundsForRestore();

    gfx::Rect window_bounds = ScreenUtil::ConvertRectToScreen(
        window_,
        window->GetTargetBounds());
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
// WorkspaceLayoutManager, ash::ShellObserver implementation:

void WorkspaceLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  const gfx::Rect work_area(ScreenUtil::ConvertRectFromScreen(
      window_,
      Shell::GetScreen()->GetDisplayNearestWindow(window_).work_area()));
  if (work_area != work_area_in_parent_) {
    const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&event);
  }
  if (backdrop_delegate_)
    backdrop_delegate_->OnDisplayWorkAreaInsetsChanged();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::WindowObserver implementation:

void WorkspaceLayoutManager::OnWindowHierarchyChanged(
    const WindowObserver::HierarchyChangeParams& params) {
  if (!wm::GetWindowState(params.target)->IsActive())
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

void WorkspaceLayoutManager::OnWindowPropertyChanged(Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    GetRootWindowController(window->GetRootWindow())->
        always_on_top_controller()->GetContainer(window)->AddChild(window);
  }
}

void WorkspaceLayoutManager::OnWindowStackingChanged(aura::Window* window) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowStackingChanged(window);
}

void WorkspaceLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = NULL;
  }
}

void WorkspaceLayoutManager::OnWindowBoundsChanged(aura::Window* window,
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

void WorkspaceLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                               aura::Window* lost_active) {
  wm::WindowState* window_state = wm::GetWindowState(gained_active);
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

  work_area_in_parent_ = ScreenUtil::ConvertRectFromScreen(
      window_,
      Shell::GetScreen()->GetDisplayNearestWindow(window_).work_area());

  // Don't do any adjustments of the insets while we are in screen locked mode.
  // This would happen if the launcher was auto hidden before the login screen
  // was shown and then gets shown when the login screen gets presented.
  if (event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED &&
      Shell::GetInstance()->session_state_delegate()->IsScreenLocked())
    return;

  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (WindowSet::const_iterator it = windows_.begin();
       it != windows_.end();
       ++it) {
    wm::GetWindowState(*it)->OnWMEvent(event);
  }
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  if (shelf_)
    shelf_->UpdateVisibilityState();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  // TODO(flackr): The fullscreen state is currently tracked per workspace
  // but the shell notification implies a per root window state. Currently
  // only windows in the default workspace container will go fullscreen but
  // this should really be tracked by the RootWindowController since
  // technically any container could get a fullscreen window.
  if (!shelf_)
    return;
  bool is_fullscreen = GetRootWindowController(
      window_->GetRootWindow())->GetWindowForFullscreenMode() != NULL;
  if (is_fullscreen != is_fullscreen_) {
    ash::Shell::GetInstance()->NotifyFullscreenStateChange(
        is_fullscreen, window_->GetRootWindow());
    is_fullscreen_ = is_fullscreen;
  }
}

}  // namespace ash
