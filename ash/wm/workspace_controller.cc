// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller.h"

#include "ash/ash_switches.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace/workspace_manager2.h"
#include "base/command_line.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

WorkspaceController::WorkspaceController(aura::Window* viewport)
    : viewport_(viewport),
      layout_manager_(NULL),
      event_handler_(NULL) {
  aura::RootWindow* root_window = viewport->GetRootWindow();
  if (IsWorkspace2Enabled()) {
    WorkspaceManager2* workspace_manager = new WorkspaceManager2(viewport);
    workspace_manager_.reset(workspace_manager);
  } else {
    WorkspaceManager* workspace_manager = new WorkspaceManager(viewport);
    workspace_manager_.reset(workspace_manager);
    layout_manager_ = new WorkspaceLayoutManager(
        root_window, workspace_manager);
    viewport->SetLayoutManager(layout_manager_);
    event_handler_ = new WorkspaceEventHandler(viewport);
    viewport->AddPreTargetHandler(event_handler_);
  }
  aura::client::GetActivationClient(root_window)->AddObserver(this);
}

WorkspaceController::~WorkspaceController() {
  aura::client::GetActivationClient(viewport_->GetRootWindow())->
      RemoveObserver(this);
  // WorkspaceLayoutManager may attempt to access state from us. Destroy it now.
  if (layout_manager_ && viewport_->layout_manager() == layout_manager_)
    viewport_->SetLayoutManager(NULL);
}

// static
bool WorkspaceController::IsWorkspace2Enabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshDisableWorkspace2);
}

bool WorkspaceController::IsInMaximizedMode() const {
  return workspace_manager_->IsInMaximizedMode();
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
