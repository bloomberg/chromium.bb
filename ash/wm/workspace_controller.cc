// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_filter.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

namespace {

// Size of the grid when a grid is enabled.
const int kGridSize = 16;

}  // namespace

WorkspaceController::WorkspaceController(aura::Window* viewport)
    : viewport_(viewport),
      layout_manager_(NULL),
      event_filter_(NULL) {
  aura::RootWindow* root_window = viewport->GetRootWindow();
  event_filter_ = new WorkspaceEventFilter(viewport);
  viewport->SetEventFilter(event_filter_);
  WorkspaceManager* workspace_manager = new WorkspaceManager(viewport);
  workspace_manager_.reset(workspace_manager);
  layout_manager_ = new WorkspaceLayoutManager(root_window, workspace_manager);
  viewport->SetLayoutManager(layout_manager_);
  aura::client::GetActivationClient(root_window)->AddObserver(this);
  SetGridSize(kGridSize);
}

WorkspaceController::~WorkspaceController() {
  aura::client::GetActivationClient(viewport_->GetRootWindow())->
      RemoveObserver(this);
  // WorkspaceLayoutManager may attempt to access state from us. Destroy it now.
  if (viewport_->layout_manager() == layout_manager_)
    viewport_->SetLayoutManager(NULL);
}

bool WorkspaceController::IsInMaximizedMode() const {
  return workspace_manager_->IsInMaximizedMode();
}

void WorkspaceController::SetGridSize(int grid_size) {
  workspace_manager_->SetGridSize(grid_size);
  if (event_filter_)
    event_filter_->set_grid_size(grid_size);
}

int WorkspaceController::GetGridSize() const {
  return workspace_manager_->GetGridSize();
}

WorkspaceWindowState WorkspaceController::GetWindowState() const {
  return workspace_manager_->GetWindowState();
}

void WorkspaceController::SetShelf(ShelfLayoutManager* shelf) {
  workspace_manager_->SetShelf(shelf);
}

void WorkspaceController::SetActiveWorkspaceByWindow(aura::Window* window) {
  return workspace_manager_->SetActiveWorkspaceByWindow(window);
}

aura::Window* WorkspaceController::GetParentForNewWindow(aura::Window* window) {
  return workspace_manager_->GetParentForNewWindow(window);
}


void WorkspaceController::OnWindowActivated(aura::Window* window,
                                            aura::Window* old_active) {
  if (!window || window->GetRootWindow() == viewport_->GetRootWindow())
    workspace_manager_->SetActiveWorkspaceByWindow(window);
}

}  // namespace internal
}  // namespace ash
