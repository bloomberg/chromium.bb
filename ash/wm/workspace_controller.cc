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
      workspace_manager_(new WorkspaceManager(viewport)),
      layout_manager_(NULL),
      event_filter_(NULL) {
  aura::RootWindow* root_window = viewport->GetRootWindow();
  event_filter_ = new WorkspaceEventFilter(viewport);
  viewport->SetEventFilter(event_filter_);
  layout_manager_ = new WorkspaceLayoutManager(
      root_window, workspace_manager_.get());
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

void WorkspaceController::SetGridSize(int grid_size) {
  workspace_manager_->set_grid_size(grid_size);
  event_filter_->set_grid_size(grid_size);
}

void WorkspaceController::OnWindowActivated(aura::Window* window,
                                            aura::Window* old_active) {
  workspace_manager_->SetActiveWorkspaceByWindow(window);
}

}  // namespace internal
}  // namespace ash
