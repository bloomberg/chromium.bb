// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/screen_ash.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/native_widget_aura.h"

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, public:

WorkspaceLayoutManager::WorkspaceLayoutManager(
    aura::RootWindow* root_window,
    WorkspaceManager* workspace_manager)
    : BaseLayoutManager(root_window),
      workspace_manager_(workspace_manager) {
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
}

void WorkspaceLayoutManager::OnWindowResized() {
  // Workspace is updated via OnRootWindowResized.
}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  BaseLayoutManager::OnWindowAddedToLayout(child);
  if (!workspace_manager_->IsManagedWindow(child))
    return;

  if (child->IsVisible()) {
    workspace_manager_->AddWindow(child);
  } else if (wm::IsWindowNormal(child)) {
    // Align non-maximized/fullscreen windows to a grid.
    SetChildBoundsDirect(
        child, workspace_manager_->AlignBoundsToGrid(child->GetTargetBounds()));
  }
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  workspace_manager_->RemoveWindow(child);
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (!workspace_manager_->IsManagedWindow(child))
    return;
  if (visible)
    workspace_manager_->AddWindow(child);
  else
    workspace_manager_->RemoveWindow(child);
}

void WorkspaceLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  if (GetTrackedByWorkspace(child))
    BaseLayoutManager::SetChildBounds(child, requested_bounds);
  else
    SetChildBoundsDirect(child, requested_bounds);
}

void WorkspaceLayoutManager::OnRootWindowResized(const gfx::Size& new_size) {
  workspace_manager_->SetWorkspaceSize(new_size);
}

void WorkspaceLayoutManager::OnScreenWorkAreaInsetsChanged() {
  workspace_manager_->OnScreenWorkAreaInsetsChanged();
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  BaseLayoutManager::OnWindowPropertyChanged(window, key, old);
  if (key == aura::client::kShowStateKey) {
    workspace_manager_->ShowStateChanged(window);
  } else if (key == ash::kWindowTrackedByWorkspaceSplitPropKey &&
             ash::GetTrackedByWorkspace(window)) {
    // We currently don't need to support transitioning from true to false, so
    // we ignore it.
    workspace_manager_->AddWindow(window);
  }
}

}  // namespace internal
}  // namespace ash
