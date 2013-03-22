// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/visibility_controller.h"

namespace ash {
namespace internal {

Workspace::Workspace(WorkspaceManager* manager,
                     aura::Window* parent,
                     bool is_maximized)
    : is_maximized_(is_maximized),
      workspace_manager_(manager),
      window_(new aura::Window(NULL)),
      event_handler_(new WorkspaceEventHandler(window_)),
      workspace_layout_manager_(NULL) {
  views::corewm::SetChildWindowVisibilityChangesAnimated(window_);
  SetWindowVisibilityAnimationTransition(window_, views::corewm::ANIMATE_NONE);
  window_->set_id(kShellWindowId_WorkspaceContainer);
  window_->SetName("WorkspaceContainer");
  window_->Init(ui::LAYER_NOT_DRAWN);
  // Do this so when animating out windows don't extend beyond the bounds.
  window_->layer()->SetMasksToBounds(true);
  window_->Hide();
  parent->AddChild(window_);
  window_->SetProperty(internal::kUsesScreenCoordinatesKey, true);

  // The layout-manager cannot be created in the initializer list since it
  // depends on the window to have been initialized.
  workspace_layout_manager_ = new WorkspaceLayoutManager(this);
  window_->SetLayoutManager(workspace_layout_manager_);
}

Workspace::~Workspace() {
  // ReleaseWindow() should have been invoked before we're deleted.
  DCHECK(!window_);
}

aura::Window* Workspace::GetTopmostActivatableWindow() {
  for (aura::Window::Windows::const_reverse_iterator i =
           window_->children().rbegin();
       i != window_->children().rend();
       ++i) {
    if (wm::CanActivateWindow(*i))
      return (*i);
  }
  return NULL;
}

aura::Window* Workspace::ReleaseWindow() {
  // Remove the LayoutManager and EventFilter as they refer back to us and/or
  // WorkspaceManager.
  window_->SetLayoutManager(NULL);
  window_->SetEventFilter(NULL);
  aura::Window* window = window_;
  window_ = NULL;
  return window;
}

bool Workspace::ShouldMoveToPending() const {
  if (!is_maximized_)
    return false;

  for (size_t i = 0; i < window_->children().size(); ++i) {
    aura::Window* child(window_->children()[i]);
    if (!child->TargetVisibility() || wm::IsWindowMinimized(child))
      continue;

    if (!GetTrackedByWorkspace(child)) {
      // If we have a maximized window that isn't tracked don't move to
      // pending. This handles the case of dragging a maximized window.
      if (WorkspaceManager::IsMaximized(child))
        return false;
      continue;
    }

    if (!GetPersistsAcrossAllWorkspaces(child))
      return false;
  }
  return true;
}

int Workspace::GetNumMaximizedWindows() const {
  int count = 0;
  for (size_t i = 0; i < window_->children().size(); ++i) {
    aura::Window* child = window_->children()[i];
    if (GetTrackedByWorkspace(child) &&
        (WorkspaceManager::IsMaximized(child) ||
         WorkspaceManager::WillRestoreMaximized(child))) {
      if (++count == 2)
        return count;
    }
  }
  return count;
}

}  // namespace internal
}  // namespace ash
