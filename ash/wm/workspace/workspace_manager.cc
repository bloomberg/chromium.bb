// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager.h"

#include <algorithm>

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/managed_workspace.h"
#include "ash/wm/workspace/maximized_workspace.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace {

// Returns a list of all the windows with layers in |result|.
void BuildWindowList(const std::vector<aura::Window*>& windows,
                     std::vector<aura::Window*>* result) {
  for (size_t i = 0; i < windows.size(); ++i) {
    if (windows[i]->layer())
      result->push_back(windows[i]);
    BuildWindowList(windows[i]->transient_children(), result);
  }
}

}

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WindowManager, public:

WorkspaceManager::WorkspaceManager(aura::Window* contents_view)
    : contents_view_(contents_view),
      active_workspace_(NULL),
      ignored_window_(NULL),
      grid_size_(0),
      shelf_(NULL) {
  DCHECK(contents_view);
}

WorkspaceManager::~WorkspaceManager() {
  std::vector<Workspace*> copy_to_delete(workspaces_);
  STLDeleteElements(&copy_to_delete);
}

bool WorkspaceManager::IsManagedWindow(aura::Window* window) const {
  return window->type() == aura::client::WINDOW_TYPE_NORMAL &&
         !window->transient_parent() &&
         ash::GetTrackedByWorkspace(window) &&
         !ash::GetPersistsAcrossAllWorkspaces(window);
}

bool WorkspaceManager::IsManagingWindow(aura::Window* window) const {
  return FindBy(window) != NULL;
}

bool WorkspaceManager::IsInMaximizedMode() const {
  return active_workspace_ &&
      active_workspace_->type() == Workspace::TYPE_MAXIMIZED;
}

void WorkspaceManager::AddWindow(aura::Window* window) {
  DCHECK(IsManagedWindow(window));

  Workspace* current_workspace = FindBy(window);
  if (current_workspace) {
    // Already know about this window. Make sure the workspace is active.
    if (active_workspace_ != current_workspace) {
      if (active_workspace_)
        window->layer()->GetAnimator()->StopAnimating();
      current_workspace->Activate();
    }
    window->Show();
    UpdateShelfVisibility();
    return;
  }

  Workspace* workspace = NULL;
  Workspace::Type type_for_window = Workspace::TypeForWindow(window);
  switch (type_for_window) {
    case Workspace::TYPE_MANAGED:
      // All normal windows go in the same workspace.
      workspace = GetManagedWorkspace();
      break;

    case Workspace::TYPE_MAXIMIZED:
      // All maximized windows go in their own workspace.
      break;
  }

  if (!workspace)
    workspace = CreateWorkspace(type_for_window);
  workspace->AddWindowAfter(window, NULL);
  workspace->Activate();
  UpdateShelfVisibility();
}

void WorkspaceManager::RemoveWindow(aura::Window* window) {
  Workspace* workspace = FindBy(window);
  if (!workspace)
    return;
  workspace->RemoveWindow(window);
  CleanupWorkspace(workspace);
}

void WorkspaceManager::SetActiveWorkspaceByWindow(aura::Window* window) {
  Workspace* workspace = FindBy(window);
  if (workspace)
    workspace->Activate();
}

void WorkspaceManager::UpdateShelfVisibility() {
  shelf_->UpdateVisibilityState();
}

WorkspaceManager::WindowState WorkspaceManager::GetWindowState() {
  if (!shelf_ || !active_workspace_)
    return WINDOW_STATE_DEFAULT;

  // TODO: this code needs to be made multi-monitor aware.
  gfx::Rect shelf_bounds(shelf_->GetIdealBounds());
  const aura::Window::Windows& windows(contents_view_->children());
  bool window_overlaps_launcher = false;
  bool has_maximized_window = false;
  for (aura::Window::Windows::const_iterator i = windows.begin();
       i != windows.end(); ++i) {
    ui::Layer* layer = (*i)->layer();
    if (!layer->GetTargetVisibility() || layer->GetTargetOpacity() == 0.0f)
      continue;
    if (wm::IsWindowMaximized(*i)) {
      // An untracked window may still be fullscreen so we keep iterating when
      // we hit a maximized window.
      has_maximized_window = true;
    } else if (wm::IsWindowFullscreen(*i)) {
      return WINDOW_STATE_FULL_SCREEN;
    }
    if (!window_overlaps_launcher && (*i)->bounds().Intersects(shelf_bounds))
      window_overlaps_launcher = true;
  }
  if (has_maximized_window)
    return WINDOW_STATE_MAXIMIZED;

  return window_overlaps_launcher ? WINDOW_STATE_WINDOW_OVERLAPS_SHELF :
      WINDOW_STATE_DEFAULT;
}

void WorkspaceManager::ShowStateChanged(aura::Window* window) {
  if (!IsManagedWindow(window) || !FindBy(window))
    return;

  Workspace::Type old_type = FindBy(window)->type();
  Workspace::Type new_type = Workspace::TypeForWindow(window);
  if (new_type != old_type)
    OnTypeOfWorkspacedNeededChanged(window);
  UpdateShelfVisibility();
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceManager, private:

void WorkspaceManager::AddWorkspace(Workspace* workspace) {
  DCHECK(std::find(workspaces_.begin(), workspaces_.end(),
                   workspace) == workspaces_.end());
  if (active_workspace_) {
    // New workspaces go right after current workspace.
    Workspaces::iterator i = std::find(workspaces_.begin(), workspaces_.end(),
                                       active_workspace_);
    workspaces_.insert(++i, workspace);
  } else {
    workspaces_.push_back(workspace);
  }
}

void WorkspaceManager::RemoveWorkspace(Workspace* workspace) {
  Workspaces::iterator i = std::find(workspaces_.begin(),
                                     workspaces_.end(),
                                     workspace);
  DCHECK(i != workspaces_.end());
  i = workspaces_.erase(i);
  if (active_workspace_ == workspace) {
    // TODO: need mru order.
    if (i != workspaces_.end())
      SetActiveWorkspace(*i);
    else if (!workspaces_.empty())
      SetActiveWorkspace(workspaces_.back());
    else
      active_workspace_ = NULL;
  }
}

void WorkspaceManager::SetVisibilityOfWorkspaceWindows(
    ash::internal::Workspace* workspace,
    AnimateChangeType change_type,
    bool value) {
  std::vector<aura::Window*> children;
  BuildWindowList(workspace->windows(), &children);
  SetWindowLayerVisibility(children, change_type, value);
}

void WorkspaceManager::SetWindowLayerVisibility(
    const std::vector<aura::Window*>& windows,
    AnimateChangeType change_type,
    bool value) {
  for (size_t i = 0; i < windows.size(); ++i) {
    ui::Layer* layer = windows[i]->layer();
    // Only show the layer for windows that want to be visible.
    if (layer && (!value || windows[i]->TargetVisibility())) {
      bool animation_disabled =
          windows[i]->GetProperty(aura::client::kAnimationsDisabledKey);
      WindowVisibilityAnimationType animation_type =
          GetWindowVisibilityAnimationType(windows[i]);
      windows[i]->SetProperty(aura::client::kAnimationsDisabledKey,
                              change_type == DONT_ANIMATE);
      bool update_layer = true;
      if (change_type == ANIMATE) {
        ash::SetWindowVisibilityAnimationType(
            windows[i],
            value ? ash::WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_SHOW :
                    ash::WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_HIDE);
        if (ash::internal::AnimateOnChildWindowVisibilityChanged(
                windows[i], value))
          update_layer = false;
      }
      if (update_layer)
        layer->SetVisible(value);
      // Reset the animation type so it isn't used in a future hide/show.
      ash::SetWindowVisibilityAnimationType(
          windows[i], animation_type);
      windows[i]->SetProperty(aura::client::kAnimationsDisabledKey,
                              animation_disabled);
    }
  }
}

Workspace* WorkspaceManager::GetActiveWorkspace() const {
  return active_workspace_;
}

Workspace* WorkspaceManager::FindBy(aura::Window* window) const {
  int index = GetWorkspaceIndexContaining(window);
  return index < 0 ? NULL : workspaces_[index];
}

void WorkspaceManager::SetActiveWorkspace(Workspace* workspace) {
  if (active_workspace_ == workspace)
    return;
  DCHECK(std::find(workspaces_.begin(), workspaces_.end(),
                   workspace) != workspaces_.end());
  if (active_workspace_)
    SetVisibilityOfWorkspaceWindows(active_workspace_, ANIMATE, false);
  Workspace* last_active = active_workspace_;
  active_workspace_ = workspace;
  if (active_workspace_) {
    SetVisibilityOfWorkspaceWindows(active_workspace_,
                                    last_active ? ANIMATE : DONT_ANIMATE, true);
    UpdateShelfVisibility();
  }
}

// Returns the index of the workspace that contains the |window|.
int WorkspaceManager::GetWorkspaceIndexContaining(aura::Window* window) const {
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       ++i) {
    if ((*i)->Contains(window))
      return i - workspaces_.begin();
  }
  return -1;
}

void WorkspaceManager::SetWindowBounds(aura::Window* window,
                                       const gfx::Rect& bounds) {
  ignored_window_ = window;
  window->SetBounds(bounds);
  ignored_window_ = NULL;
}

void WorkspaceManager::OnTypeOfWorkspacedNeededChanged(aura::Window* window) {
  DCHECK(IsManagedWindow(window));
  Workspace* current_workspace = FindBy(window);
  DCHECK(current_workspace);
  Workspace* new_workspace = NULL;
  if (Workspace::TypeForWindow(window) == Workspace::TYPE_MAXIMIZED) {
    // Unmaximized -> maximized; create a new workspace.
    current_workspace->RemoveWindow(window);
    new_workspace = CreateWorkspace(Workspace::TYPE_MAXIMIZED);
    new_workspace->AddWindowAfter(window, NULL);
  } else {
    // Maximized -> unmaximized; move window to unmaximized workspace.
    new_workspace = GetManagedWorkspace();
    current_workspace->RemoveWindow(window);
    if (!new_workspace)
      new_workspace = CreateWorkspace(Workspace::TYPE_MANAGED);
    new_workspace->AddWindowAfter(window, NULL);
  }
  SetActiveWorkspace(new_workspace);
  // Delete at the end so that we don't attempt to switch to another
  // workspace in RemoveWorkspace().
  CleanupWorkspace(current_workspace);
}

Workspace* WorkspaceManager::GetManagedWorkspace() {
  for (size_t i = 0; i < workspaces_.size(); ++i) {
    if (workspaces_[i]->type() == Workspace::TYPE_MANAGED)
      return workspaces_[i];
  }
  return NULL;
}

Workspace* WorkspaceManager::CreateWorkspace(Workspace::Type type) {
  Workspace* workspace = NULL;
  if (type == Workspace::TYPE_MAXIMIZED)
    workspace = new MaximizedWorkspace(this);
  else
    workspace = new ManagedWorkspace(this);
  AddWorkspace(workspace);
  return workspace;
}

void WorkspaceManager::CleanupWorkspace(Workspace* workspace) {
  if (workspace->type() != Workspace::TYPE_MANAGED && workspace->is_empty())
    delete workspace;
}

}  // namespace internal
}  // namespace ash
