// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
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

// static
gfx::Rect BaseLayoutManager::BoundsWithScreenEdgeVisible(
    aura::Window* window,
    const gfx::Rect& restore_bounds) {
  gfx::Rect max_bounds =
      ash::ScreenAsh::GetMaximizedWindowBoundsInParent(window);
  // If the restore_bounds are more than 1 grid step away from the size the
  // window would be when maximized, inset it.
  max_bounds.Inset(ash::internal::WorkspaceWindowResizer::kScreenEdgeInset,
                   ash::internal::WorkspaceWindowResizer::kScreenEdgeInset);
  if (restore_bounds.Contains(max_bounds))
    return max_bounds;
  return restore_bounds;
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
    UpdateBoundsFromShowState(child, false);
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
                       child->GetProperty(internal::kRestoreShowStateKey));
    child->ClearProperty(internal::kRestoreShowStateKey);
  }
}

void BaseLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  // Some windows rely on this to set their initial bounds.
  if (wm::IsWindowMaximized(child))
    child_bounds = ScreenAsh::GetMaximizedWindowBoundsInParent(child);
  else if (wm::IsWindowFullscreen(child))
    child_bounds = ScreenAsh::GetDisplayBoundsInParent(child);
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

void BaseLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  AdjustWindowSizesForScreenChange();
}

/////////////////////////////////////////////////////////////////////////////
// BaseLayoutManager, WindowObserver overrides:

void BaseLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
  if (key == aura::client::kShowStateKey) {
    ui::WindowShowState old_state = static_cast<ui::WindowShowState>(old);
    ui::WindowShowState new_state =
        window->GetProperty(aura::client::kShowStateKey);
    if (old_state != new_state && old_state != ui::SHOW_STATE_MINIMIZED &&
        !GetRestoreBoundsInScreen(window) &&
        ((new_state == ui::SHOW_STATE_MAXIMIZED &&
          old_state != ui::SHOW_STATE_FULLSCREEN) ||
         (new_state == ui::SHOW_STATE_FULLSCREEN &&
          old_state != ui::SHOW_STATE_MAXIMIZED))) {
      SetRestoreBoundsInParent(window, window->bounds());
    }
    // Minimized state handles its own animations.
    // TODO(sky): get animations to work with Workspace2.
    bool animate = (old_state != ui::SHOW_STATE_MINIMIZED) &&
        !WorkspaceController::IsWorkspace2Enabled();
    UpdateBoundsFromShowState(window, animate);
    ShowStateChanged(window, old_state);
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
    window->SetProperty(internal::kRestoreShowStateKey, last_show_state);
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

void BaseLayoutManager::UpdateBoundsFromShowState(aura::Window* window,
                                                  bool animate) {
  switch (window->GetProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBoundsInScreen(window);
      if (restore) {
        gfx::Rect bounds_in_parent =
            ScreenAsh::ConvertRectFromScreen(window->parent(), *restore);
        MaybeAnimateToBounds(window,
                             animate,
                             BoundsWithScreenEdgeVisible(window,
                                                         bounds_in_parent));
      }
      window->ClearProperty(aura::client::kRestoreBoundsKey);
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
      MaybeAnimateToBounds(window,
                           animate,
                           ScreenAsh::GetMaximizedWindowBoundsInParent(window));
      break;

    case ui::SHOW_STATE_FULLSCREEN:
      // Don't animate the full-screen window transition.
      // TODO(jamescook): Use animation here.  Be sure the lock screen works.
      SetChildBoundsDirect(
          window, ScreenAsh::GetDisplayBoundsInParent(window));
      break;

    default:
      break;
  }
}

void BaseLayoutManager::MaybeAnimateToBounds(aura::Window* window,
                                             bool animate,
                                             const gfx::Rect& new_bounds) {
  // Only animate visible windows.
  if (animate &&
      window->TargetVisibility() &&
      !window->GetProperty(aura::client::kAnimationsDisabledKey) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshWindowAnimationsDisabled)) {
    CrossFadeToBounds(window, new_bounds);
    return;
  }
  SetChildBoundsDirect(window, new_bounds);
}

void BaseLayoutManager::AdjustWindowSizesForScreenChange() {
  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (WindowSet::const_iterator it = windows_.begin();
       it != windows_.end();
       ++it) {
    aura::Window* window = *it;
    if (wm::IsWindowMaximized(window)) {
      SetChildBoundsDirect(
          window, ScreenAsh::GetMaximizedWindowBoundsInParent(window));
    } else if (wm::IsWindowFullscreen(window)) {
      SetChildBoundsDirect(
          window, ScreenAsh::GetDisplayBoundsInParent(window));
    } else {
      // The work area may be smaller than the full screen.
      gfx::Rect display_rect =
          ScreenAsh::GetDisplayWorkAreaBoundsInParent(window);
      // Put as much of the window as possible within the display area.
      window->SetBounds(window->bounds().AdjustToFit(display_rect));
    }
  }
}

}  // namespace internal
}  // namespace ash
