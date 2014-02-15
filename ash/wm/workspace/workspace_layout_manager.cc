// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/display/display_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"
#include "ui/views/corewm/window_util.h"

using aura::Window;

namespace ash {

namespace internal {

namespace {

// This specifies how much percent 30% of a window rect (width / height)
// must be visible when the window is added to the workspace.
const float kMinimumPercentOnScreenArea = 0.3f;

}  // namespace

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
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  if (root_window_)
    root_window_->RemoveObserver(this);
  for (WindowSet::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
}

void WorkspaceLayoutManager::SetShelf(internal::ShelfLayoutManager* shelf) {
  shelf_ = shelf;
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::LayoutManager implementation:

void WorkspaceLayoutManager::OnWindowAddedToLayout(Window* child) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  AdjustWindowBoundsWhenAdded(window_state);

  windows_.insert(child);
  child->AddObserver(this);
  window_state->AddObserver(this);
  // TODO(oshima): This is necessary as the call in
  // AdjustWindowBoundsWhenAdded is skipped when the bounds is
  // empty. Investigate if we can eliminate this dup.
  if (!window_state->is_dragged())
    SetMaximizedOrFullscreenBounds(window_state);
  UpdateShelfVisibility();
  UpdateFullscreenState();
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
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(Window* child,
                                                            bool visible) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  // Attempting to show a minimized window. Unminimize it.
  if (visible && window_state->IsMinimized())
    window_state->Unminimize();

  if (child->TargetVisibility()) {
    WindowPositioner::RearrangeVisibleWindowOnShow(child);
  } else {
    if (wm::GetWindowState(child)->IsFullscreen())
      UpdateFullscreenState();
    WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(child);
  }
  UpdateShelfVisibility();
}

void WorkspaceLayoutManager::SetChildBounds(
    Window* child,
    const gfx::Rect& requested_bounds) {
  wm::WindowState* window_state = wm::GetWindowState(child);
  if (window_state->is_dragged()) {
    SetChildBoundsDirect(child, requested_bounds);
  } else if (window_state->IsSnapped()) {
    gfx::Rect child_bounds(requested_bounds);
    wm::AdjustBoundsSmallerThan(work_area_in_parent_.size(), &child_bounds);
    window_state->AdjustSnappedBounds(&child_bounds);
    SetChildBoundsDirect(child, child_bounds);
  } else if (!SetMaximizedOrFullscreenBounds(window_state)) {
    // Some windows rely on this to set their initial bounds.
    // Non-maximized/full-screen windows have their size constrained to the
    // work-area.
    gfx::Rect child_bounds(requested_bounds);
    wm::AdjustBoundsSmallerThan(work_area_in_parent_.size(), &child_bounds);
    SetChildBoundsDirect(child, child_bounds);
  }
  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, ash::ShellObserver implementation:

void WorkspaceLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  const gfx::Rect work_area(ScreenUtil::ConvertRectFromScreen(
      window_,
      Shell::GetScreen()->GetDisplayNearestWindow(window_).work_area()));
  if (work_area != work_area_in_parent_) {
    AdjustAllWindowsBoundsForWorkAreaChange(
        ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED);
  }
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, aura::WindowObserver implementation:

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
  if (root_window_ == window)
    AdjustAllWindowsBoundsForWorkAreaChange(ADJUST_WINDOW_DISPLAY_SIZE_CHANGED);
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
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, wm::WindowStateObserver implementation:

void WorkspaceLayoutManager::OnPostWindowShowTypeChange(
    wm::WindowState* window_state,
    wm::WindowShowType old_type) {

  // Notify observers that fullscreen state may be changing.
  if (window_state->IsFullscreen() || old_type == wm::SHOW_TYPE_FULLSCREEN)
    UpdateFullscreenState();

  UpdateShelfVisibility();
}

//////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, private:

void WorkspaceLayoutManager::AdjustAllWindowsBoundsForWorkAreaChange(
    AdjustWindowReason reason) {
  work_area_in_parent_ = ScreenUtil::ConvertRectFromScreen(
      window_,
      Shell::GetScreen()->GetDisplayNearestWindow(window_).work_area());

  // Don't do any adjustments of the insets while we are in screen locked mode.
  // This would happen if the launcher was auto hidden before the login screen
  // was shown and then gets shown when the login screen gets presented.
  if (reason == ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED &&
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
    AdjustWindowBoundsForWorkAreaChange(wm::GetWindowState(*it), reason);
  }
}

void WorkspaceLayoutManager::AdjustWindowBoundsForWorkAreaChange(
    wm::WindowState* window_state,
    AdjustWindowReason reason) {
  if (window_state->is_dragged())
    return;

  // Do not cross fade here: the window's layer hierarchy may be messed up for
  // the transition between mirroring and extended. See also: crbug.com/267698
  // TODO(oshima): Differentiate display change and shelf visibility change, and
  // bring back CrossFade animation.
  if (window_state->IsMaximized() &&
      reason == ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED) {
    SetChildBoundsDirect(window_state->window(),
                         ScreenUtil::GetMaximizedWindowBoundsInParent(
                             window_state->window()));
    return;
  }

  if (SetMaximizedOrFullscreenBounds(window_state))
    return;

  gfx::Rect bounds = window_state->window()->bounds();
  switch (reason) {
    case ADJUST_WINDOW_DISPLAY_SIZE_CHANGED:
      // The work area may be smaller than the full screen.  Put as much of the
      // window as possible within the display area.
      bounds.AdjustToFit(work_area_in_parent_);
      break;
    case ADJUST_WINDOW_WORK_AREA_INSETS_CHANGED:
      ash::wm::AdjustBoundsToEnsureMinimumWindowVisibility(
          work_area_in_parent_, &bounds);
      break;
  }
  window_state->AdjustSnappedBounds(&bounds);
  if (window_state->window()->bounds() != bounds)
    window_state->SetBoundsDirectAnimated(bounds);
}

void WorkspaceLayoutManager::AdjustWindowBoundsWhenAdded(
    wm::WindowState* window_state) {
  // Don't adjust window bounds if the bounds are empty as this
  // happens when a new views::Widget is created.
  // When a window is dragged and dropped onto a different
  // root window, the bounds will be updated after they are added
  // to the root window.
  if (window_state->window()->bounds().IsEmpty() ||
      window_state->is_dragged() ||
      SetMaximizedOrFullscreenBounds(window_state)) {
    return;
  }

  Window* window = window_state->window();
  gfx::Rect bounds = window->bounds();

  // Use entire display instead of workarea because the workarea can
  // be further shrunk by the docked area. The logic ensures 30%
  // visibility which should be enough to see where the window gets
  // moved.
  gfx::Rect display_area = ScreenUtil::GetDisplayBoundsInParent(window);

  int min_width = bounds.width() * kMinimumPercentOnScreenArea;
  int min_height = bounds.height() * kMinimumPercentOnScreenArea;
  ash::wm::AdjustBoundsToEnsureWindowVisibility(
      display_area, min_width, min_height, &bounds);
  window_state->AdjustSnappedBounds(&bounds);
  if (window->bounds() != bounds)
    window->SetBounds(bounds);
}

void WorkspaceLayoutManager::UpdateShelfVisibility() {
  if (shelf_)
    shelf_->UpdateVisibilityState();
}

void WorkspaceLayoutManager::UpdateFullscreenState() {
  bool is_fullscreen = GetRootWindowController(
      window_->GetRootWindow())->GetWindowForFullscreenMode() != NULL;
  if (is_fullscreen != is_fullscreen_) {
    ash::Shell::GetInstance()->NotifyFullscreenStateChange(
        is_fullscreen, window_->GetRootWindow());
    is_fullscreen_ = is_fullscreen;
  }
}

bool WorkspaceLayoutManager::SetMaximizedOrFullscreenBounds(
    wm::WindowState* window_state) {
  DCHECK(!window_state->is_dragged());

  if (window_state->IsMaximized()) {
    SetChildBoundsDirect(
        window_state->window(), ScreenUtil::GetMaximizedWindowBoundsInParent(
            window_state->window()));
    return true;
  }
  if (window_state->IsFullscreen()) {
    SetChildBoundsDirect(
        window_state->window(),
        ScreenUtil::GetDisplayBoundsInParent(window_state->window()));
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace ash
