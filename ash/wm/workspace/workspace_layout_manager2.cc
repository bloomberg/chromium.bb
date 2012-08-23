// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager2.h"

#include "ash/screen_ash.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/workspace/workspace2.h"
#include "ash/wm/workspace/workspace_manager2.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/event.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

using aura::Window;

namespace ash {
namespace internal {

WorkspaceLayoutManager2::WorkspaceLayoutManager2(aura::RootWindow* root_window,
                                                 Workspace2* workspace)
    : BaseLayoutManager(root_window),
      workspace_(workspace) {
}

WorkspaceLayoutManager2::~WorkspaceLayoutManager2() {
}

void WorkspaceLayoutManager2::OnWindowAddedToLayout(Window* child) {
  BaseLayoutManager::OnWindowAddedToLayout(child);
  workspace_manager()->OnWindowAddedToWorkspace(workspace_, child);
}

void WorkspaceLayoutManager2::OnWillRemoveWindowFromLayout(Window* child) {
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
  workspace_manager()->OnWillRemoveWindowFromWorkspace(workspace_, child);
}

void WorkspaceLayoutManager2::OnWindowRemovedFromLayout(Window* child) {
  BaseLayoutManager::OnWindowRemovedFromLayout(child);
  workspace_manager()->OnWindowRemovedFromWorkspace(workspace_, child);
}

void WorkspaceLayoutManager2::OnChildWindowVisibilityChanged(Window* child,
                                                             bool visibile) {
  BaseLayoutManager::OnChildWindowVisibilityChanged(child, visibile);
  workspace_manager()->OnWorkspaceChildWindowVisibilityChanged(workspace_,
                                                               child);
}

void WorkspaceLayoutManager2::SetChildBounds(
    Window* child,
    const gfx::Rect& requested_bounds) {
  BaseLayoutManager::SetChildBounds(child, requested_bounds);
  workspace_manager()->OnWorkspaceWindowChildBoundsChanged(workspace_, child);
}

void WorkspaceLayoutManager2::OnWindowPropertyChanged(Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  BaseLayoutManager::OnWindowPropertyChanged(window, key, old);

  if (key == aura::client::kAlwaysOnTopKey &&
      window->GetProperty(aura::client::kAlwaysOnTopKey)) {
    internal::AlwaysOnTopController* controller =
        window->GetRootWindow()->GetProperty(
            internal::kAlwaysOnTopControllerKey);
    controller->GetContainer(window)->AddChild(window);
  }
}

void WorkspaceLayoutManager2::ShowStateChanged(
    Window* window,
    ui::WindowShowState last_show_state) {
  // NOTE: we can't use BaseLayoutManager::ShowStateChanged() as we need to
  // forward to WorkspaceManager before the window is hidden.
  if (wm::IsWindowMinimized(window)) {
    // Save the previous show state so that we can correctly restore it.
    window->SetProperty(internal::kRestoreShowStateKey, last_show_state);
    SetWindowVisibilityAnimationType(
        window, WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

    workspace_manager()->OnWorkspaceWindowShowStateChanged(
        workspace_, window, last_show_state);

    // Hide the window.
    window->Hide();

    // Activate another window.
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
        workspace_, window, last_show_state);
  }
}

WorkspaceManager2* WorkspaceLayoutManager2::workspace_manager() {
  return workspace_->workspace_manager();
}

}  // namespace internal
}  // namespace ash
