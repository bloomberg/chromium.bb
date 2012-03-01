// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_manager.h"
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

WorkspaceLayoutManager::~WorkspaceLayoutManager() {}

void WorkspaceLayoutManager::OnWindowResized() {
  // Workspace is updated via RootWindowObserver::OnRootWindowResized.
}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
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
  if (wm::IsWindowMaximized(child)) {
    child_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(child);
  } else if (wm::IsWindowFullscreen(child)) {
    child_bounds = gfx::Screen::GetMonitorAreaNearestWindow(child);
  } else {
    child_bounds = gfx::Screen::GetMonitorWorkAreaNearestWindow(child).
        AdjustToFit(requested_bounds);
  }
  SetChildBoundsDirect(child, child_bounds);
}

}  // namespace internal
}  // namespace ash
