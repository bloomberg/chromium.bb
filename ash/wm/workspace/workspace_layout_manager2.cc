// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager2.h"

#include "ash/ash_switches.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace2.h"
#include "ash/wm/workspace/workspace_manager2.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event.h"
#include "ui/base/ui_base_types.h"

using aura::Window;

namespace ash {
namespace internal {

WorkspaceLayoutManager2::WorkspaceLayoutManager2(Workspace2* workspace)
    : root_window_(workspace->window()->GetRootWindow()),
      workspace_(workspace),
      work_area_(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                     workspace->window()->parent())) {
  Shell::GetInstance()->AddShellObserver(this);
  root_window_->AddRootWindowObserver(this);
  root_window_->AddObserver(this);
}

WorkspaceLayoutManager2::~WorkspaceLayoutManager2() {
  if (root_window_) {
    root_window_->RemoveObserver(this);
    root_window_->RemoveRootWindowObserver(this);
  }
  for (WindowSet::const_iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void WorkspaceLayoutManager2::OnWindowAddedToLayout(Window* child) {
  windows_.insert(child);
  child->AddObserver(this);

  // Only update the bounds if the window has a show state that depends on the
  // workspace area.
  if (wm::IsWindowMaximized(child) || wm::IsWindowFullscreen(child))
    UpdateBoundsFromShowState(child);

  workspace_manager()->OnWindowAddedToWorkspace(workspace_, child);
}

void WorkspaceLayoutManager2::OnWillRemoveWindowFromLayout(Window* child) {
  windows_.erase(child);
  child->RemoveObserver(this);
  workspace_manager()->OnWillRemoveWindowFromWorkspace(workspace_, child);
}

void WorkspaceLayoutManager2::OnWindowRemovedFromLayout(Window* child) {
  workspace_manager()->OnWindowRemovedFromWorkspace(workspace_, child);
}

void WorkspaceLayoutManager2::OnChildWindowVisibilityChanged(Window* child,
                                                             bool visible) {
  if (visible && wm::IsWindowMinimized(child)) {
    // Attempting to show a minimized window. Unminimize it.
    child->SetProperty(aura::client::kShowStateKey,
                       child->GetProperty(internal::kRestoreShowStateKey));
    child->ClearProperty(internal::kRestoreShowStateKey);
  }
  workspace_manager()->OnWorkspaceChildWindowVisibilityChanged(workspace_,
                                                               child);
}

void WorkspaceLayoutManager2::SetChildBounds(
    Window* child,
    const gfx::Rect& requested_bounds) {
  gfx::Rect child_bounds(requested_bounds);
  // Some windows rely on this to set their initial bounds.
  if (!SetMaximizedOrFullscreenBounds(child))
    SetChildBoundsDirect(child, child_bounds);
  workspace_manager()->OnWorkspaceWindowChildBoundsChanged(workspace_, child);
}

void WorkspaceLayoutManager2::OnRootWindowResized(const aura::RootWindow* root,
                                                  const gfx::Size& old_size) {
  AdjustWindowSizesForScreenChange();
}

void WorkspaceLayoutManager2::OnDisplayWorkAreaInsetsChanged() {
  if (workspace_manager()->active_workspace_ == workspace_) {
    const gfx::Rect work_area(ScreenAsh::GetDisplayWorkAreaBoundsInParent(
                                  workspace_->window()->parent()));
    if (work_area != work_area_)
      AdjustWindowSizesForScreenChange();
  }
}

void WorkspaceLayoutManager2::OnWindowPropertyChanged(Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  if (key == aura::client::kShowStateKey) {
    ui::WindowShowState old_state = static_cast<ui::WindowShowState>(old);
    ui::WindowShowState new_state =
        window->GetProperty(aura::client::kShowStateKey);
    if (old_state != ui::SHOW_STATE_MINIMIZED &&
        GetRestoreBoundsInScreen(window) == NULL &&
        WorkspaceManager2::IsMaximizedState(new_state) &&
        !WorkspaceManager2::IsMaximizedState(old_state)) {
      SetRestoreBoundsInParent(window, window->bounds());
    }
    // When restoring from a minimized state, we want to restore to the
    // previous (maybe L/R maximized) state. Since we do also want to keep the
    // restore rectangle, we set the restore rectangle to the rectangle we want
    // to restore to and restore it after we switched so that it is preserved.
    gfx::Rect restore;
    if (old_state == ui::SHOW_STATE_MINIMIZED &&
        (new_state == ui::SHOW_STATE_NORMAL ||
         new_state == ui::SHOW_STATE_DEFAULT) &&
        GetRestoreBoundsInScreen(window)) {
      restore = *GetRestoreBoundsInScreen(window);
      SetRestoreBoundsInScreen(window, window->bounds());
    }

    // If maximizing or restoring, clone the layer. WorkspaceManager will use it
    // (and take ownership of it) when animating. Ideally we could use that of
    // BaseLayoutManager, but that proves problematic. In particular when
    // restoring we need to animate on top of the workspace animating in.
    ui::Layer* cloned_layer = NULL;
    if (wm::IsActiveWindow(window) &&
        ((WorkspaceManager2::IsMaximizedState(new_state) &&
          wm::IsWindowStateNormal(old_state)) ||
         (!WorkspaceManager2::IsMaximizedState(new_state) &&
          WorkspaceManager2::IsMaximizedState(old_state) &&
          new_state != ui::SHOW_STATE_MINIMIZED))) {
      cloned_layer = wm::RecreateWindowLayers(window, false);
    }
    UpdateBoundsFromShowState(window);
    ShowStateChanged(window, old_state, cloned_layer);

    // Set the restore rectangle to the previously set restore rectangle.
    if (!restore.IsEmpty())
      SetRestoreBoundsInScreen(window, restore);
  }

  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    internal::AlwaysOnTopController* controller =
        window->GetRootWindow()->GetProperty(
            internal::kAlwaysOnTopControllerKey);
    controller->GetContainer(window)->AddChild(window);
  }
}

void WorkspaceLayoutManager2::OnWindowDestroying(aura::Window* window) {
  if (root_window_ == window) {
    root_window_->RemoveObserver(this);
    root_window_ = NULL;
  }
}

void WorkspaceLayoutManager2::ShowStateChanged(
    Window* window,
    ui::WindowShowState last_show_state,
    ui::Layer* cloned_layer) {
  if (wm::IsWindowMinimized(window)) {
    DCHECK(!cloned_layer);
    // Save the previous show state so that we can correctly restore it.
    window->SetProperty(internal::kRestoreShowStateKey, last_show_state);
    SetWindowVisibilityAnimationType(
        window, WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
    workspace_manager()->OnWorkspaceWindowShowStateChanged(
        workspace_, window, last_show_state, NULL);
    window->Hide();
    if (wm::IsActiveWindow(window))
      wm::DeactivateWindow(window);
  } else {
    if ((window->TargetVisibility() ||
         last_show_state == ui::SHOW_STATE_MINIMIZED) &&
        !window->layer()->visible()) {
      // The layer may be hidden if the window was previously minimized. Make
      // sure it's visible.
      window->Show();
    }
    workspace_manager()->OnWorkspaceWindowShowStateChanged(
        workspace_, window, last_show_state, cloned_layer);
  }
}

void WorkspaceLayoutManager2::AdjustWindowSizesForScreenChange() {
  work_area_ = ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      workspace_->window()->parent());
  // If a user plugs an external display into a laptop running Aura the
  // display size will change.  Maximized windows need to resize to match.
  // We also do this when developers running Aura on a desktop manually resize
  // the host window.
  // We also need to do this when the work area insets changes.
  for (WindowSet::const_iterator it = windows_.begin();
       it != windows_.end();
       ++it) {
    AdjustWindowSizeForScreenChange(*it);
  }
}

void WorkspaceLayoutManager2::AdjustWindowSizeForScreenChange(Window* window) {
  if (!SetMaximizedOrFullscreenBounds(window)) {
    // The work area may be smaller than the full screen.  Put as much of the
    // window as possible within the display area.
    window->SetBounds(window->bounds().AdjustToFit(work_area_));
  }
}

void WorkspaceLayoutManager2::UpdateBoundsFromShowState(Window* window) {
  // See comment in SetMaximizedOrFullscreenBounds() as to why we use parent in
  // these calculation.
  switch (window->GetProperty(aura::client::kShowStateKey)) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL: {
      const gfx::Rect* restore = GetRestoreBoundsInScreen(window);
      if (restore) {
        gfx::Rect bounds_in_parent =
            ScreenAsh::ConvertRectFromScreen(window->parent()->parent(),
                                             *restore);
        SetChildBoundsDirect(
            window,
            BaseLayoutManager::BoundsWithScreenEdgeVisible(
                window->parent()->parent(),
                bounds_in_parent));
      }
      window->ClearProperty(aura::client::kRestoreBoundsKey);
      break;
    }

    case ui::SHOW_STATE_MAXIMIZED:
    case ui::SHOW_STATE_FULLSCREEN:
      SetMaximizedOrFullscreenBounds(window);
      break;

    default:
      break;
  }
}

bool WorkspaceLayoutManager2::SetMaximizedOrFullscreenBounds(
    aura::Window* window) {
  // During animations there is a transform installed on the workspace
  // windows. For this reason this code uses the parent so that the transform is
  // ignored.
  if (wm::IsWindowMaximized(window)) {
    SetChildBoundsDirect(
        window, ScreenAsh::GetMaximizedWindowBoundsInParent(
            window->parent()->parent()));
    return true;
  }
  if (wm::IsWindowFullscreen(window)) {
    SetChildBoundsDirect(
        window,
        ScreenAsh::GetDisplayBoundsInParent(window->parent()->parent()));
    return true;
  }
  return false;
}

WorkspaceManager2* WorkspaceLayoutManager2::workspace_manager() {
  return workspace_->workspace_manager();
}

}  // namespace internal
}  // namespace ash
