// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

WorkspaceController::WorkspaceController(aura::Window* viewport)
    : workspace_manager_(new WorkspaceManager(viewport)),
      launcher_model_(NULL),
      ignore_move_event_(false) {
  aura::RootWindow::GetInstance()->AddRootWindowObserver(this);
  aura::RootWindow::GetInstance()->AddObserver(this);
}

WorkspaceController::~WorkspaceController() {
  if (launcher_model_)
    launcher_model_->RemoveObserver(this);
  aura::RootWindow::GetInstance()->RemoveObserver(this);
  aura::RootWindow::GetInstance()->RemoveRootWindowObserver(this);
}

void WorkspaceController::ToggleOverview() {
  workspace_manager_->SetOverview(!workspace_manager_->is_overview());
}

void WorkspaceController::SetLauncherModel(LauncherModel* launcher_model) {
  DCHECK(!launcher_model_);
  launcher_model_ = launcher_model;
  launcher_model_->AddObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceController, aura::RootWindowObserver overrides:

void WorkspaceController::OnRootWindowResized(const gfx::Size& new_size) {
  workspace_manager_->SetWorkspaceSize(new_size);
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceController, aura::WindowObserver overrides:

void WorkspaceController::OnWindowPropertyChanged(aura::Window* window,
                                                  const char* key,
                                                  void* old) {
  if (key == aura::client::kRootWindowActiveWindow)
    workspace_manager_->SetActiveWorkspaceByWindow(GetActiveWindow());
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceController, ash::LauncherModelObserver overrides:

void WorkspaceController::LauncherItemAdded(int index) {
}

void WorkspaceController::LauncherItemRemoved(int index) {
}

void WorkspaceController::LauncherItemMoved(int start_index, int target_index) {
  if (ignore_move_event_)
    return;
  // TODO: there is no longer a 1-1 mapping between the launcher and windows;
  // decide how we want to handle it.
  DCHECK(launcher_model_);
  NOTIMPLEMENTED();
}

void WorkspaceController::LauncherItemChanged(
    int index,
    const ash::LauncherItem& old_item) {
}

}  // namespace internal
}  // namespace ash
