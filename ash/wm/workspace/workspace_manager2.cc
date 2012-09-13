// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager2.h"

#include <algorithm>
#include <functional>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/system_background_controller.h"
#include "ash/wm/workspace/workspace_layout_manager2.h"
#include "ash/wm/workspace/workspace2.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

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

WorkspaceType WorkspaceType(Workspace2* workspace) {
  return workspace->is_maximized() ? WORKSPACE_MAXIMIZED : WORKSPACE_DESKTOP;
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
      shelf_(NULL),
      in_move_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          clear_unminimizing_workspace_factory_(this)) {
  // Clobber any existing event filter.
  contents_view->SetEventFilter(NULL);
  // |contents_view| takes ownership of WorkspaceManagerLayoutManager2.
  contents_view->SetLayoutManager(
      new WorkspaceManagerLayoutManager2(contents_view));
  active_workspace_ = CreateWorkspace(false);
  workspaces_.push_back(active_workspace_);
  active_workspace_->window()->Show();
}

WorkspaceManager2::~WorkspaceManager2() {
  // Release the windows, they'll be destroyed when |contents_view_| is
  // destroyed.
  std::for_each(workspaces_.begin(), workspaces_.end(),
                std::mem_fun(&Workspace2::ReleaseWindow));
  std::for_each(pending_workspaces_.begin(), pending_workspaces_.end(),
                std::mem_fun(&Workspace2::ReleaseWindow));
  std::for_each(to_delete_.begin(), to_delete_.end(),
                std::mem_fun(&Workspace2::ReleaseWindow));
  STLDeleteElements(&workspaces_);
  STLDeleteElements(&pending_workspaces_);
  STLDeleteElements(&to_delete_);
}

// static
bool WorkspaceManager2::IsMaximized(Window* window) {
  return IsMaximizedState(window->GetProperty(aura::client::kShowStateKey));
}

// static
bool WorkspaceManager2::IsMaximizedState(ui::WindowShowState state) {
  return state == ui::SHOW_STATE_MAXIMIZED ||
      state == ui::SHOW_STATE_FULLSCREEN;
}

// static
bool WorkspaceManager2::WillRestoreMaximized(Window* window) {
  return wm::IsWindowMinimized(window) &&
      IsMaximizedState(window->GetProperty(internal::kRestoreShowStateKey));
}

bool WorkspaceManager2::IsInMaximizedMode() const {
  return active_workspace_ && active_workspace_->is_maximized();
}

WorkspaceWindowState WorkspaceManager2::GetWindowState() const {
  if (!shelf_)
    return WORKSPACE_WINDOW_STATE_DEFAULT;

  const bool is_active_maximized = active_workspace_->is_maximized();
  const gfx::Rect shelf_bounds(shelf_->GetIdealBounds());
  const Window::Windows& windows(active_workspace_->window()->children());
  bool window_overlaps_launcher = false;
  bool has_maximized_window = false;
  for (Window::Windows::const_iterator i = windows.begin();
       i != windows.end(); ++i) {
    if (GetIgnoredByShelf(*i))
      continue;
    ui::Layer* layer = (*i)->layer();
    if (!layer->GetTargetVisibility() || layer->GetTargetOpacity() == 0.0f)
      continue;
    // Ignore maximized/fullscreen windows if we're in the desktop. Such a state
    // is transitory and means we haven't yet switched. If we did consider such
    // windows we'll return the wrong thing, which can lead to prematurely
    // changing the launcher state and clobbering restore bounds.
    if (is_active_maximized) {
      if (wm::IsWindowMaximized(*i)) {
        // An untracked window may still be fullscreen so we keep iterating when
        // we hit a maximized window.
        has_maximized_window = true;
      } else if (wm::IsWindowFullscreen(*i)) {
        return WORKSPACE_WINDOW_STATE_FULL_SCREEN;
      }
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
      SetActiveWorkspace(workspace, ANIMATE_OLD_AND_NEW);
  }
}

Window* WorkspaceManager2::GetParentForNewWindow(Window* window) {
  if (window->transient_parent()) {
    DCHECK(contents_view_->Contains(window->transient_parent()));
    DCHECK(!IsMaximized(window));
    return window->transient_parent()->parent();
  }

  if (IsMaximized(window)) {
    // Wait for the window to be made active before showing the workspace.
    Workspace2* workspace = CreateWorkspace(true);
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

void WorkspaceManager2::SetActiveWorkspace(Workspace2* workspace,
                                           AnimateType animate_type) {
  DCHECK(workspace);
  if (active_workspace_ == workspace)
    return;

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

  Workspace2* last_active = active_workspace_;
  active_workspace_ = workspace;

  // The display work-area may have changed while |workspace| was not the active
  // workspace. Give it a chance to adjust its state for the new work-area.
  active_workspace_->workspace_layout_manager()->
      OnDisplayWorkAreaInsetsChanged();

  const bool is_unminimizing_maximized_window =
      unminimizing_workspace_ && unminimizing_workspace_ == active_workspace_ &&
      active_workspace_->is_maximized();
  if (is_unminimizing_maximized_window) {
    // If we're unminimizing a window it needs to be on the top, otherwise you
    // won't see the animation.
    contents_view_->StackChildAtTop(active_workspace_->window());
  } else if (active_workspace_->is_maximized() && last_active->is_maximized()) {
    // When switching between maximized windows we need the last active
    // workspace on top of the new, otherwise the animations won't look
    // right. Since only one workspace is visible at a time stacking order of
    // the workspace windows ultimately doesn't matter.
    contents_view_->StackChildAtTop(last_active->window());
  }

  destroy_background_timer_.Stop();
  if (active_workspace_ == desktop_workspace()) {
    base::TimeDelta delay(GetSystemBackgroundDestroyDuration());
    if (ui::LayerAnimator::slow_animation_mode())
      delay *= ui::LayerAnimator::slow_animation_scale_factor();
    // Delay an extra 100ms to make sure everything settlts down before
    // destroying the background.
    delay += base::TimeDelta::FromMilliseconds(100);
    destroy_background_timer_.Start(
        FROM_HERE, delay, this, &WorkspaceManager2::DestroySystemBackground);
  } else if (!background_controller_.get()) {
    background_controller_.reset(new SystemBackgroundController(
                                     contents_view_->GetRootWindow()));
  }

  UpdateShelfVisibility();

  if (animate_type != ANIMATE_NONE) {
    AnimateBetweenWorkspaces(
        last_active->window(),
        WorkspaceType(last_active),
        (animate_type == ANIMATE_OLD_AND_NEW),
        workspace->window(),
        WorkspaceType(workspace),
        is_unminimizing_maximized_window);
  }

  RootWindowController* root_controller = GetRootWindowController(
      contents_view_->GetRootWindow());
  if (root_controller) {
    aura::Window* background = root_controller->GetContainer(
        kShellWindowId_DesktopBackgroundContainer);
    if (last_active == desktop_workspace()) {
      AnimateWorkspaceOut(background, WORKSPACE_ANIMATE_DOWN,
                          WORKSPACE_DESKTOP);
    } else if (active_workspace_ == desktop_workspace()) {
      AnimateWorkspaceIn(background, WORKSPACE_ANIMATE_UP);
    }
  }
}

WorkspaceManager2::Workspaces::iterator
WorkspaceManager2::FindWorkspace(Workspace2* workspace)  {
  return std::find(workspaces_.begin(), workspaces_.end(), workspace);
}

Workspace2* WorkspaceManager2::CreateWorkspace(bool maximized) {
  return new Workspace2(this, contents_view_, maximized);
}

void WorkspaceManager2::MoveWorkspaceToPendingOrDelete(
    Workspace2* workspace,
    Window* stack_beneath,
    AnimateType animate_type) {
  // We're all ready moving windows.
  if (in_move_)
    return;

  DCHECK_NE(desktop_workspace(), workspace);

  if (workspace == active_workspace_)
    SelectNextWorkspace(animate_type);

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
    if (workspace == unminimizing_workspace_)
      unminimizing_workspace_ = NULL;
    pending_workspaces_.erase(workspace);
    ScheduleDelete(workspace);
  } else {
    pending_workspaces_.insert(workspace);
  }
}

void WorkspaceManager2::SelectNextWorkspace(AnimateType animate_type) {
  DCHECK_NE(active_workspace_, desktop_workspace());

  Workspaces::const_iterator workspace_i(FindWorkspace(active_workspace_));
  Workspaces::const_iterator next_workspace_i(workspace_i + 1);
  if (next_workspace_i != workspaces_.end())
    SetActiveWorkspace(*next_workspace_i, animate_type);
  else
    SetActiveWorkspace(*(workspace_i - 1), animate_type);
}

void WorkspaceManager2::ScheduleDelete(Workspace2* workspace) {
  to_delete_.insert(workspace);
  delete_timer_.Stop();
  delete_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                      &WorkspaceManager2::ProcessDeletion);
}

void WorkspaceManager2::DestroySystemBackground() {
  background_controller_.reset();
}

void WorkspaceManager2::SetUnminimizingWorkspace(Workspace2* workspace) {
  // The normal sequence of unminimizing a window is: Show() the window, which
  // triggers changing the kShowStateKey to NORMAL and lastly the window is made
  // active. This means at the time the window is unminimized we don't know if
  // the workspace it is in is going to become active. To track this
  // |unminimizing_workspace_| is set at the time we unminimize and a task is
  // schedule to reset it. This way when we get the activate we know we're in
  // the process unminimizing and can do the right animation.
  unminimizing_workspace_ = workspace;
  if (unminimizing_workspace_) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&WorkspaceManager2::SetUnminimizingWorkspace,
                   clear_unminimizing_workspace_factory_.GetWeakPtr(),
                   static_cast<Workspace2*>(NULL)));
  }
}

void WorkspaceManager2::ProcessDeletion() {
  std::set<Workspace2*> to_delete;
  to_delete.swap(to_delete_);
  for (std::set<Workspace2*>::iterator i = to_delete.begin();
       i != to_delete.end(); ++i) {
    Workspace2* workspace = *i;
    if (workspace->window()->layer()->children().empty()) {
      delete workspace->ReleaseWindow();
      delete workspace;
    } else {
      to_delete_.insert(workspace);
    }
  }
  if (!to_delete_.empty()) {
    delete_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                        &WorkspaceManager2::ProcessDeletion);
  }
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
    MoveWorkspaceToPendingOrDelete(workspace, NULL, ANIMATE_NEW);
}

void WorkspaceManager2::OnWorkspaceChildWindowVisibilityChanged(
    Workspace2* workspace,
    Window* child) {
  if (workspace->ShouldMoveToPending())
    MoveWorkspaceToPendingOrDelete(workspace, NULL, ANIMATE_NEW);
  else if (workspace == active_workspace_)
    UpdateShelfVisibility();
}

void WorkspaceManager2::OnWorkspaceWindowChildBoundsChanged(
    Workspace2* workspace,
    Window* child) {
  if (workspace == active_workspace_)
    UpdateShelfVisibility();
}

void WorkspaceManager2::OnWorkspaceWindowShowStateChanged(
    Workspace2* workspace,
    Window* child,
    ui::WindowShowState last_show_state,
    ui::Layer* old_layer) {
  if (wm::IsWindowMinimized(child)) {
    if (workspace->ShouldMoveToPending())
      MoveWorkspaceToPendingOrDelete(workspace, NULL, ANIMATE_NEW);
    DCHECK(!old_layer);
  } else {
    // Set of cases to deal with:
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
        DCHECK(!is_active || old_layer);
        MoveWorkspaceToPendingOrDelete(workspace, child, ANIMATE_NONE);
        if (FindWorkspace(workspace) == workspaces_.end())
          workspace = NULL;
        new_workspace = desktop_workspace();
      }
    } else if ((max_count == 1 && workspace == desktop_workspace()) ||
               max_count > 1) {
      new_workspace = CreateWorkspace(true);
      pending_workspaces_.insert(new_workspace);
      ReparentWindow(child, new_workspace->window(), NULL);
    }
    if (is_active && new_workspace) {
      DCHECK(old_layer);
      SetActiveWorkspace(new_workspace, ANIMATE_NONE);
      CrossFadeWindowBetweenWorkspaces(
          workspace ? workspace->window() : NULL, new_workspace->window(),
          child, old_layer);
    } else {
      if (last_show_state == ui::SHOW_STATE_MINIMIZED)
        SetUnminimizingWorkspace(new_workspace ? new_workspace : workspace);
      DCHECK(!old_layer);
    }
  }
  UpdateShelfVisibility();
}

}  // namespace internal
}  // namespace ash
