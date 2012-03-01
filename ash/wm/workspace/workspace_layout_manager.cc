// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
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
    WorkspaceManager* workspace_manager)
    : workspace_manager_(workspace_manager) {
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
  const aura::Window::Windows& windows(
      workspace_manager_->contents_view()->children());
  for (size_t i = 0; i < windows.size(); ++i)
    windows[i]->RemoveObserver(this);
}

void WorkspaceLayoutManager::OnWindowResized() {
  // Workspace is updated via RootWindowObserver::OnRootWindowResized.
}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  child->AddObserver(this);
  if (!workspace_manager_->IsManagedWindow(child))
    return;

  if (child->IsVisible()) {
    workspace_manager_->AddWindow(child);
  } else if (wm::IsWindowMaximized(child)) {
    SetChildBoundsDirect(child,
                         gfx::Screen::GetMonitorWorkAreaNearestWindow(child));
  } else if (wm::IsWindowFullscreen(child)) {
    SetChildBoundsDirect(child,
                         gfx::Screen::GetMonitorAreaNearestWindow(child));
  } else {
    // Align non-maximized/fullscreen windows to a grid.
    SetChildBoundsDirect(
        child, workspace_manager_->AlignBoundsToGrid(child->GetTargetBounds()));
  }
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  child->RemoveObserver(this);
  ClearRestoreBounds(child);
  workspace_manager_->RemoveWindow(child);
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
  gfx::Rect child_bounds(requested_bounds);
  if (GetTrackedByWorkspace(child)) {
    if (wm::IsWindowMaximized(child)) {
      child_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(child);
    } else if (wm::IsWindowFullscreen(child)) {
      child_bounds = gfx::Screen::GetMonitorAreaNearestWindow(child);
    } else {
      child_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(child).
          AdjustToFit(requested_bounds);
    }
  }
  SetChildBoundsDirect(child, child_bounds);
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == ash::kWindowTrackedByWorkspaceSplitPropKey &&
      ash::GetTrackedByWorkspace(window)) {
    // We currently don't need to support transitioning from true to false, so
    // we ignore it.
    workspace_manager_->AddWindow(window);
  }
}

}  // namespace internal
}  // namespace ash
