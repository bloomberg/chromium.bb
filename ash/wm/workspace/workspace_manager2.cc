// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager2.h"

#include <algorithm>
#include <functional>

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager2.h"
#include "ash/wm/workspace/workspace2.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::Workspace2*);

using aura::Window;

namespace ash {
namespace internal {

DEFINE_WINDOW_PROPERTY_KEY(Workspace2*, kWorkspaceKey, NULL);

namespace {

// Changes the parent of |window| and all its transient children to
// |new_parent|. If |stack_beneach| is non-NULL all the windows are stacked
// beneath it.
void ReparentWindow(Window* window,
                    Window* new_parent,
                    Window* stack_beneath) {
  window->SetParent(new_parent);
  if (stack_beneath)
    new_parent->StackChildBelow(window, stack_beneath);
  for (size_t i = 0; i < window->transient_children().size(); ++i)
    ReparentWindow(window->transient_children()[i], new_parent, stack_beneath);
}

// Workspace -------------------------------------------------------------------

// LayoutManager installed on the parent window of all the Workspace windows (eg
// |WorkspaceManager2::contents_view_|).
class WorkspaceManagerLayoutManager2 : public BaseLayoutManager {
 public:
  WorkspaceManagerLayoutManager2(Window* window)
      : BaseLayoutManager(window->GetRootWindow()),
        window_(window) {
  }
  virtual ~WorkspaceManagerLayoutManager2() {}

  // Overridden from BaseWorkspaceLayoutManager:
  virtual void OnWindowResized() OVERRIDE {
    for (size_t i = 0; i < window_->children().size(); ++i)
      window_->children()[i]->SetBounds(gfx::Rect(window_->bounds().size()));
  }
  virtual void OnWindowAddedToLayout(Window* child) OVERRIDE {
    // Only workspaces should be added as children.
    DCHECK_EQ(kShellWindowId_WorkspaceContainer, child->id());
    child->SetBounds(gfx::Rect(window_->bounds().size()));
  }

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManagerLayoutManager2);
};

}  // namespace

// WorkspaceManager2 -----------------------------------------------------------

WorkspaceManager2::WorkspaceManager2(Window* contents_view)
    : contents_view_(contents_view),
      active_workspace_(NULL),
      grid_size_(0),
      shelf_(NULL),
      in_move_(false) {
  // Clobber any existing event filter.
  contents_view->SetEventFilter(NULL);
  // |contents_view| takes ownership of WorkspaceManagerLayoutManager2.
  contents_view->SetLayoutManager(
      new WorkspaceManagerLayoutManager2(contents_view));
  active_workspace_ = CreateWorkspace(false);
  workspaces_.push_back(active_workspace_);
}

WorkspaceManager2::~WorkspaceManager2() {
  // Release the windows, they'll be destroyed when |contents_view_| is
  // destroyed.
  std::for_each(workspaces_.begin(), workspaces_.end(),
                std::mem_fun(&Workspace2::ReleaseWindow));
  std::for_each(pending_workspaces_.begin(), pending_workspaces_.end(),
                std::mem_fun(&Workspace2::ReleaseWindow));
  STLDeleteElements(&workspaces_);
  STLDeleteElements(&pending_workspaces_);
}

// static
bool WorkspaceManager2::IsMaximized(Window* window) {
  return wm::IsWindowFullscreen(window) || wm::IsWindowMaximized(window);
}

// static
bool WorkspaceManager2::WillRestoreMaximized(aura::Window* window) {
  if (!wm::IsWindowMinimized(window))
    return false;

  ui::WindowShowState restore_state =
      window->GetProperty(internal::kRestoreShowStateKey);
  return restore_state == ui::SHOW_STATE_MAXIMIZED ||
      restore_state == ui::SHOW_STATE_FULLSCREEN;

}

bool WorkspaceManager2::IsInMaximizedMode() const {
  return active_workspace_ && active_workspace_->is_maximized();
}

void WorkspaceManager2::SetGridSize(int size) {
  grid_size_ = size;
  std::for_each(workspaces_.begin(), workspaces_.end(),
                std::bind2nd(std::mem_fun(&Workspace2::SetGridSize),
                             grid_size_));
  std::for_each(pending_workspaces_.begin(), pending_workspaces_.end(),
                std::bind2nd(std::mem_fun(&Workspace2::SetGridSize),
                             grid_size_));
}

int WorkspaceManager2::GetGridSize() const {
  return grid_size_;
}

WorkspaceWindowState WorkspaceManager2::GetWindowState() const {
  if (!shelf_)
    return WORKSPACE_WINDOW_STATE_DEFAULT;

  gfx::Rect shelf_bounds(shelf_->GetIdealBounds());
  const Window::Windows& windows(active_workspace_->window()->children());
  bool window_overlaps_launcher = false;
  bool has_maximized_window = false;
  for (Window::Windows::const_iterator i = windows.begin();
       i != windows.end(); ++i) {
    ui::Layer* layer = (*i)->layer();
    if (!layer->GetTargetVisibility() || layer->GetTargetOpacity() == 0.0f)
      continue;
    if (wm::IsWindowMaximized(*i)) {
      // An untracked window may still be fullscreen so we keep iterating when
      // we hit a maximized window.
      has_maximized_window = true;
    } else if (wm::IsWindowFullscreen(*i)) {
      return WORKSPACE_WINDOW_STATE_FULL_SCREEN;
    }
    if (!window_overlaps_launcher && (*i)->bounds().Intersects(shelf_bounds))
      window_overlaps_launcher = true;
  }
  if (has_maximized_window)
    return WORKSPACE_WINDOW_STATE_MAXIMIZED;

  return window_overlaps_launcher ?
      WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF :
      WORKSPACE_WINDOW_STATE_DEFAULT;
}

void WorkspaceManager2::SetShelf(ShelfLayoutManager* shelf) {
  shelf_ = shelf;
}

void WorkspaceManager2::SetActiveWorkspaceByWindow(Window* window) {
  Workspace2* workspace = FindBy(window);
  if (!workspace)
    return;

  if (workspace != active_workspace_) {
    // If the window persists across all workspaces, move it to the current
    // workspace.
    if (GetPersistsAcrossAllWorkspaces(window) && !IsMaximized(window))
      ReparentWindow(window, active_workspace_->window(), NULL);
    else
      SetActiveWorkspace(workspace);
  }
  UpdateShelfVisibility();
}

Window* WorkspaceManager2::GetParentForNewWindow(Window* window) {
  if (window->transient_parent()) {
    DCHECK(contents_view_->Contains(window->transient_parent()));
    DCHECK(!IsMaximized(window));
    return window->transient_parent()->parent();
  }

  if (IsMaximized(window)) {
    // Wait for the window to be made active before showing the workspace.
    Workspace2* workspace = CreateWorkspace(false);
    pending_workspaces_.insert(workspace);
    return workspace->window();
  }

  if (!GetTrackedByWorkspace(window) || GetPersistsAcrossAllWorkspaces(window))
    return active_workspace_->window();

  return desktop_workspace()->window();
}

void WorkspaceManager2::UpdateShelfVisibility() {
  if (shelf_)
    shelf_->UpdateVisibilityState();
}

Workspace2* WorkspaceManager2::FindBy(Window* window) const {
  while (window) {
    Workspace2* workspace = window->GetProperty(kWorkspaceKey);
    if (workspace)
      return workspace;
    window = window->transient_parent();
  }
  return NULL;
}

void WorkspaceManager2::SetActiveWorkspace(Workspace2* workspace) {
  DCHECK(workspace);
  if (active_workspace_ == workspace)
    return;

  // TODO: sort out animations.

  pending_workspaces_.erase(workspace);

  // Adjust the z-order. No need to adjust the z-order for the desktop since
  // it always stays at the bottom.
  if (workspace != desktop_workspace()) {
    if (FindWorkspace(workspace) == workspaces_.end()) {
      contents_view_->StackChildAbove(workspace->window(),
                                      workspaces_.back()->window());
      workspaces_.push_back(workspace);
    }
  }

  active_workspace_ = workspace;

  for (size_t i = 0; i < workspaces_.size(); ++i) {
    if (workspaces_[i] == active_workspace_)
      workspaces_[i]->window()->Show();
    else
      workspaces_[i]->window()->Hide();
  }
}

WorkspaceManager2::Workspaces::iterator
WorkspaceManager2::FindWorkspace(Workspace2* workspace)  {
  return std::find(workspaces_.begin(), workspaces_.end(), workspace);
}

Workspace2* WorkspaceManager2::CreateWorkspace(bool maximized) {
  Workspace2* workspace = new Workspace2(this, contents_view_, maximized);
  workspace->SetGridSize(grid_size_);
  workspace->window()->SetLayoutManager(
      new WorkspaceLayoutManager2(contents_view_->GetRootWindow(), workspace));
  return workspace;
}

void WorkspaceManager2::MoveWorkspaceToPendingOrDelete(
    Workspace2* workspace,
    Window* stack_beneath) {
  // We're all ready moving windows.
  if (in_move_)
    return;

  DCHECK_NE(desktop_workspace(), workspace);

  if (workspace == active_workspace_)
    SelectNextWorkspace();

  AutoReset<bool> setter(&in_move_, true);

  // Move all non-maximized/fullscreen windows to the desktop.
  {
    // Build the list of windows to move. Exclude maximized/fullscreen and
    // windows with transient parents.
    Window::Windows to_move;
    Window* w_window = workspace->window();
    for (size_t i = 0; i < w_window->children().size(); ++i) {
      Window* child = w_window->children()[i];
      if (!child->transient_parent() && !IsMaximized(child) &&
          !WillRestoreMaximized(child)) {
        to_move.push_back(child);
      }
    }
    // Move the windows, but make sure the window is still a child of |w_window|
    // before moving (moving may cascade and cause other windows to move).
    for (size_t i = 0; i < to_move.size(); ++i) {
      if (std::find(w_window->children().begin(), w_window->children().end(),
                    to_move[i]) != w_window->children().end()) {
        ReparentWindow(to_move[i], desktop_workspace()->window(),
                       stack_beneath);
      }
    }
  }

  {
    Workspaces::iterator workspace_i(FindWorkspace(workspace));
    if (workspace_i != workspaces_.end())
      workspaces_.erase(workspace_i);
  }

  if (workspace->window()->children().empty()) {
    pending_workspaces_.erase(workspace);
    delete workspace->ReleaseWindow();
    delete workspace;
  } else {
    pending_workspaces_.insert(workspace);
  }
}

void WorkspaceManager2::SelectNextWorkspace() {
  DCHECK_NE(active_workspace_, desktop_workspace());

  Workspaces::const_iterator workspace_i(FindWorkspace(active_workspace_));
  Workspaces::const_iterator next_workspace_i(workspace_i + 1);
  if (next_workspace_i != workspaces_.end())
    SetActiveWorkspace(*next_workspace_i);
  else
    SetActiveWorkspace(*(workspace_i - 1));
  UpdateShelfVisibility();
}

void WorkspaceManager2::OnWindowAddedToWorkspace(Workspace2* workspace,
                                                 Window* child) {
  child->SetProperty(kWorkspaceKey, workspace);
  // Do nothing (other than updating shelf visibility) as the right parent was
  // chosen by way of GetParentForNewWindow() or we explicitly moved the window
  // to the workspace.
  if (workspace == active_workspace_)
    UpdateShelfVisibility();
}

void WorkspaceManager2::OnWillRemoveWindowFromWorkspace(Workspace2* workspace,
                                                        Window* child) {
  child->ClearProperty(kWorkspaceKey);
}

void WorkspaceManager2::OnWindowRemovedFromWorkspace(Workspace2* workspace,
                                                     Window* child) {
  if (workspace->ShouldMoveToPending())
    MoveWorkspaceToPendingOrDelete(workspace, NULL);
}

void WorkspaceManager2::OnWorkspaceChildWindowVisibilityChanged(
    Workspace2* workspace,
    Window* child) {
  if (workspace->ShouldMoveToPending())
    MoveWorkspaceToPendingOrDelete(workspace, NULL);
}

void WorkspaceManager2::OnWorkspaceWindowChildBoundsChanged(
    Workspace2* workspace,
    Window* child) {
}

void WorkspaceManager2::OnWorkspaceWindowShowStateChanged(
    Workspace2* workspace,
    Window* child,
    ui::WindowShowState last_show_state) {
  if (wm::IsWindowMinimized(child)) {
    if (workspace->ShouldMoveToPending())
      MoveWorkspaceToPendingOrDelete(workspace, NULL);
  } else {
    // Here's the cases that need to be handled:
    // . More than one maximized window: move newly maximized window into
    //   own workspace.
    // . One maximized window and not in a maximized workspace: move window
    //   into own workspace.
    // . No maximized window and not in desktop: move to desktop and further
    //   any existing windows are stacked beneath |child|.
    const bool is_active = wm::IsActiveWindow(child);
    Workspace2* new_workspace = NULL;
    const int max_count = workspace->GetNumMaximizedWindows();
    if (max_count == 0) {
      if (workspace != desktop_workspace()) {
        {
          AutoReset<bool> setter(&in_move_, true);
          ReparentWindow(child, desktop_workspace()->window(), NULL);
        }
        MoveWorkspaceToPendingOrDelete(workspace, child);
        new_workspace = desktop_workspace();
      }
    } else if (max_count == 1) {
      if (workspace == desktop_workspace()) {
        new_workspace = CreateWorkspace(true);
        pending_workspaces_.insert(new_workspace);
        ReparentWindow(child, new_workspace->window(), NULL);
      }
    } else {
      new_workspace = CreateWorkspace(true);
      pending_workspaces_.insert(new_workspace);
      ReparentWindow(child, new_workspace->window(), NULL);
    }
    if (is_active && new_workspace)
      SetActiveWorkspace(new_workspace);
  }
  UpdateShelfVisibility();
}

}  // namespace internal
}  // namespace ash
