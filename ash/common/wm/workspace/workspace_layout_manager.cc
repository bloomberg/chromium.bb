// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/workspace/workspace_layout_manager.h"

#include <algorithm>

#include "ash/common/keyboard/keyboard_observer_register.h"
#include "ash/common/session/session_controller.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm/always_on_top_controller.h"
#include "ash/common/wm/fullscreen_window_finder.h"
#include "ash/common/wm/window_positioner.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm/wm_screen_util.h"
#include "ash/common/wm/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

WorkspaceLayoutManager::WorkspaceLayoutManager(WmWindow* window)
    : window_(window),
      root_window_(window->GetRootWindow()),
      root_window_controller_(root_window_->GetRootWindowController()),
      shell_(window_->GetShell()),
      work_area_in_parent_(wm::GetDisplayWorkAreaBoundsInParent(window_)),
      is_fullscreen_(wm::GetWindowForFullscreenMode(window) != nullptr),
      keyboard_observer_(this) {
  Shell::GetInstance()->AddShellObserver(this);
  Shell::GetInstance()->activation_client()->AddObserver(this);
  root_window_->aura_window()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  DCHECK(window->aura_window()->GetProperty(kSnapChildrenToPixelBoundary));
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  if (root_window_)
    root_window_->aura_window()->RemoveObserver(this);
  for (WmWindow* window : windows_) {
    wm::WindowState* window_state = window->GetWindowState();
    window_state->RemoveObserver(this);
    window->aura_window()->RemoveObserver(this);
  }
  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void WorkspaceLayoutManager::SetMaximizeBackdropDelegate(
    std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate) {
  backdrop_delegate_ = std::move(delegate);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowResized() {}

void WorkspaceLayoutManager::OnWindowAddedToLayout(WmWindow* child) {
  wm::WindowState* window_state = child->GetWindowState();
  wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
  windows_.insert(child);
  child->aura_window()->AddObserver(this);
  window_state->AddObserver(this);
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowAddedToLayout(child);
  WindowPositioner::RearrangeVisibleWindowOnShow(child);
  if (WmShell::Get()->IsPinned())
    child->GetWindowState()->DisableAlwaysOnTop(nullptr);
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(WmWindow* child) {
  windows_.erase(child);
  child->aura_window()->RemoveObserver(this);
  child->GetWindowState()->RemoveObserver(this);

  if (child->GetTargetVisibility())
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
}

void WorkspaceLayoutManager::OnWindowRemovedFromLayout(WmWindow* child) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowRemovedFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(WmWindow* child,
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

void WorkspaceLayoutManager::SetChildBounds(WmWindow* child,
                                            const gfx::Rect& requested_bounds) {
  wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, requested_bounds);
  child->GetWindowState()->OnWMEvent(&event);
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, keyboard::KeyboardControllerObserver implementation:

void WorkspaceLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  // If new window behavior flag enabled and in non-sticky mode, do not change
  // the work area.
  bool change_work_area =
      (!base::CommandLine::ForCurrentProcess()->HasSwitch(
           ::switches::kUseNewVirtualKeyboardBehavior) ||
       keyboard::KeyboardController::GetInstance()->keyboard_locked());
  if (!change_work_area)
    return;

  WmWindow* window = shell_->GetActiveWindow();
  if (!window)
    return;

  window = window->GetToplevelWindow();
  if (!window_->Contains(window))
    return;

  wm::WindowState* window_state = window->GetWindowState();
  if (window_state->ignore_keyboard_bounds_change())
    return;

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

void WorkspaceLayoutManager::OnKeyboardClosed() {
  keyboard_observer_.RemoveAll();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::WindowObserver implementation:

void WorkspaceLayoutManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (!wm::GetWindowState(params.target)->IsActive())
    return;
  // If the window is already tracked by the workspace this update would be
  // redundant as the fullscreen and shelf state would have been handled in
  // OnWindowAddedToLayout.
  if (windows_.find(WmWindow::Get(params.target)) != windows_.end())
    return;

  // If the active window has moved to this root window then update the
  // fullscreen state.
  // TODO(flackr): Track the active window leaving this root window and update
  // the fullscreen state accordingly.
  if (params.new_parent &&
      WmWindow::Get(params.new_parent->GetRootWindow()) == root_window_) {
    UpdateFullscreenState();
    UpdateShelfVisibility();
  }
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    WmWindow* container =
        root_window_controller_->always_on_top_controller()->GetContainer(
            WmWindow::Get(window));
    if (WmWindow::Get(window->parent()) != container)
      container->AddChild(WmWindow::Get(window));
  }
}

void WorkspaceLayoutManager::OnWindowStackingChanged(aura::Window* window) {
  UpdateShelfVisibility();
  UpdateFullscreenState();
  if (backdrop_delegate_)
    backdrop_delegate_->OnWindowStackingChanged(WmWindow::Get(window));
}

void WorkspaceLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == WmWindow::Get(window)) {
    root_window_->aura_window()->RemoveObserver(this);
    root_window_ = nullptr;
  }
}

void WorkspaceLayoutManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (root_window_ == WmWindow::Get(window)) {
    const wm::WMEvent wm_event(wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&wm_event);
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager,
// aura::client::ActivationChangeObserver implementation:

void WorkspaceLayoutManager::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  WmWindow* wm_gained_active = WmWindow::Get(gained_active);
  wm::WindowState* window_state =
      wm_gained_active ? wm_gained_active->GetWindowState() : nullptr;
  if (window_state && window_state->IsMinimized() &&
      !wm_gained_active->IsVisible()) {
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
// WorkspaceLayoutManager, display::DisplayObserver implementation:

void WorkspaceLayoutManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (window_->GetDisplayNearestWindow().id() != display.id())
    return;

  const gfx::Rect work_area(wm::GetDisplayWorkAreaBoundsInParent(window_));
  if (work_area != work_area_in_parent_) {
    const wm::WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
    AdjustAllWindowsBoundsForWorkAreaChange(&event);
  }
  if (backdrop_delegate_)
    backdrop_delegate_->OnDisplayWorkAreaInsetsChanged();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ShellObserver implementation:

void WorkspaceLayoutManager::OnFullscreenStateChanged(bool is_fullscreen,
                                                      WmWindow* root_window) {
  if (root_window != root_window_ || is_fullscreen_ == is_fullscreen)
    return;

  is_fullscreen_ = is_fullscreen;
  if (WmShell::Get()->IsPinned()) {
    // If this is in pinned mode, then this event does not trigger the
    // always-on-top state change, because it is kept disabled regardless of
    // the fullscreen state change.
    return;
  }

  UpdateAlwaysOnTop(is_fullscreen_ ? wm::GetWindowForFullscreenMode(window_)
                                   : nullptr);
}

void WorkspaceLayoutManager::OnPinnedStateChanged(WmWindow* pinned_window) {
  if (!WmShell::Get()->IsPinned() && is_fullscreen_) {
    // On exiting from pinned mode, if the workspace is still in fullscreen
    // mode, then this event does not trigger the restoring yet. On exiting
    // from fullscreen, the temporarily disabled always-on-top property will be
    // restored.
    return;
  }

  UpdateAlwaysOnTop(WmShell::Get()->IsPinned() ? pinned_window : nullptr);
}

void WorkspaceLayoutManager::OnVirtualKeyboardStateChanged(
    bool activated,
    WmWindow* root_window) {
  UpdateKeyboardObserverFromStateChanged(activated, root_window, root_window_,
                                         &keyboard_observer_);
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, private:

void WorkspaceLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    const wm::WMEvent* event) {
  DCHECK(event->type() == wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED ||
         event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);

  work_area_in_parent_ = wm::GetDisplayWorkAreaBoundsInParent(window_);

  // Don't do any adjustments of the insets while we are in screen locked mode.
  // This would happen if the launcher was auto hidden before the login screen
  // was shown and then gets shown when the login screen gets presented.
  if (event->type() == wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED &&
      Shell::Get()->session_controller()->IsScreenLocked())
    return;

  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (WmWindow* window : windows_)
    window->GetWindowState()->OnWMEvent(event);
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  if (root_window_controller_->HasShelf())
    root_window_controller_->GetShelf()->UpdateVisibilityState();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  // TODO(flackr): The fullscreen state is currently tracked per workspace
  // but the shell notification implies a per root window state. Currently
  // only windows in the default workspace container will go fullscreen but
  // this should really be tracked by the RootWindowController since
  // technically any container could get a fullscreen window.
  if (window_->GetShellWindowId() != kShellWindowId_DefaultContainer)
    return;
  bool is_fullscreen = wm::GetWindowForFullscreenMode(window_) != nullptr;
  if (is_fullscreen != is_fullscreen_) {
    Shell::GetInstance()->NotifyFullscreenStateChanged(is_fullscreen,
                                                       root_window_);
    is_fullscreen_ = is_fullscreen;
  }
}

void WorkspaceLayoutManager::UpdateAlwaysOnTop(WmWindow* window_on_top) {
  // Changing always on top state may change window's parent. Iterate on a copy
  // of |windows_| to avoid invalidating an iterator. Since both workspace and
  // always_on_top containers' layouts are managed by this class all the
  // appropriate windows will be included in the iteration.
  WindowSet windows(windows_);
  for (auto* window : windows) {
    wm::WindowState* window_state = window->GetWindowState();
    if (window_on_top)
      window_state->DisableAlwaysOnTop(window_on_top);
    else
      window_state->RestoreAlwaysOnTop();
  }
}

}  // namespace ash
