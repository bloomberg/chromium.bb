// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager.h"

#include <algorithm>
#include <functional>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/frame_painter.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/auto_window_management.h"
#include "ash/wm/workspace/desktop_background_fade_controller.h"
#include "ash/wm/workspace/workspace_animations.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::Workspace*);
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, ui::WindowShowState);

using aura::Window;

namespace ash {
namespace internal {

DEFINE_WINDOW_PROPERTY_KEY(Workspace*, kWorkspaceKey, NULL);

namespace {

// Duration for fading out the desktop background when fullscreen.
const int kCrossFadeSwitchTimeMS = 700;

// Amount of time to pause before animating anything. Only used during initial
// animation (when logging in).
const int kInitialPauseTimeMS = 750;

// Changes the parent of |window| and all its transient children to
// |new_parent|. If |stack_beneach| is non-NULL all the windows are stacked
// beneath it.
void ReparentWindow(Window* window,
                    Window* new_parent,
                    Window* stack_beneath) {
  new_parent->AddChild(window);
  if (stack_beneath)
    new_parent->StackChildBelow(window, stack_beneath);
  for (size_t i = 0; i < window->transient_children().size(); ++i)
    ReparentWindow(window->transient_children()[i], new_parent, stack_beneath);
}

}  // namespace

// Workspace -------------------------------------------------------------------

// LayoutManager installed on the parent window of all the Workspace window (eg
// |WorkspaceManager::contents_window_|).
class WorkspaceManager::LayoutManagerImpl : public BaseLayoutManager {
 public:
  explicit LayoutManagerImpl(WorkspaceManager* workspace_manager)
      : BaseLayoutManager(
            workspace_manager->contents_window_->GetRootWindow()),
        workspace_manager_(workspace_manager) {
  }
  virtual ~LayoutManagerImpl() {}

  // Overridden from BaseWorkspaceLayoutManager:
  virtual void OnWindowResized() OVERRIDE {
    for (size_t i = 0; i < window()->children().size(); ++i)
      window()->children()[i]->SetBounds(gfx::Rect(window()->bounds().size()));
  }
  virtual void OnWindowAddedToLayout(Window* child) OVERRIDE {
    // Only workspaces should be added as children.
    DCHECK((child->id() == kShellWindowId_WorkspaceContainer) ||
           workspace_manager_->creating_fade_);
    child->SetBounds(gfx::Rect(window()->bounds().size()));
  }

 private:
  aura::Window* window() { return workspace_manager_->contents_window_; }

  WorkspaceManager* workspace_manager_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerImpl);
};

// WorkspaceManager -----------------------------------------------------------

WorkspaceManager::WorkspaceManager(Window* contents_window)
    : contents_window_(contents_window),
      active_workspace_(NULL),
      shelf_(NULL),
      in_move_(false),
      app_terminating_(false),
      creating_fade_(false) {
  // Clobber any existing event filter.
  contents_window->SetEventFilter(NULL);
  // |contents_window| takes ownership of LayoutManagerImpl.
  contents_window->SetLayoutManager(new LayoutManagerImpl(this));
  active_workspace_ = new Workspace(this, contents_window_);
  workspaces_.push_back(active_workspace_);
  active_workspace_->window()->Show();
  Shell::GetInstance()->AddShellObserver(this);
}

WorkspaceManager::~WorkspaceManager() {
  Shell::GetInstance()->RemoveShellObserver(this);
  // Release the windows, they'll be destroyed when |contents_window_| is
  // destroyed.
  std::for_each(workspaces_.begin(), workspaces_.end(),
                std::mem_fun(&Workspace::ReleaseWindow));
  std::for_each(pending_workspaces_.begin(), pending_workspaces_.end(),
                std::mem_fun(&Workspace::ReleaseWindow));
  std::for_each(to_delete_.begin(), to_delete_.end(),
                std::mem_fun(&Workspace::ReleaseWindow));
  STLDeleteElements(&workspaces_);
  STLDeleteElements(&pending_workspaces_);
  STLDeleteElements(&to_delete_);
}

WorkspaceWindowState WorkspaceManager::GetWindowState() const {
  if (!shelf_)
    return WORKSPACE_WINDOW_STATE_DEFAULT;

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

void WorkspaceManager::SetShelf(ShelfLayoutManager* shelf) {
  shelf_ = shelf;
}

void WorkspaceManager::SetActiveWorkspaceByWindow(Window* window) {
  Workspace* workspace = FindBy(window);
  if (!workspace)
    return;

  if (workspace != active_workspace_) {
    // A window is being made active. In the following case we reparent to
    // the active desktop:
    // . The window is not tracked by workspace code. This is used for tab
    //   dragging. Since tab dragging needs to happen in the active workspace we
    //   have to reparent the window (otherwise the window you dragged the tab
    //   out of would disappear since the workspace changed). Since this case is
    //   only transiently used (property reset on input release) we don't worry
    //   about window state. In fact we can't consider window state here as we
    //   have to allow dragging of a fullscreen window to work in this case.
    if (!GetTrackedByWorkspace(window))
      ReparentWindow(window, active_workspace_->window(), NULL);
    else
      SetActiveWorkspace(workspace, SWITCH_WINDOW_MADE_ACTIVE);
  }
}

Window* WorkspaceManager::GetActiveWorkspaceWindow() {
  return active_workspace_->window();
}

Window* WorkspaceManager::GetParentForNewWindow(Window* window) {
  // Try to put windows with transient parents in the same workspace as their
  // transient parent.
  if (window->transient_parent()) {
    Workspace* workspace = FindBy(window->transient_parent());
    if (workspace)
      return workspace->window();
    // Fall through to normal logic.
  }

  if (!GetTrackedByWorkspace(window))
    return active_workspace_->window();

  return desktop_workspace()->window();
}

void WorkspaceManager::DoInitialAnimation() {
  ShowWorkspace(active_workspace_, active_workspace_, SWITCH_INITIAL);
}

void WorkspaceManager::OnAppTerminating() {
  app_terminating_ = true;
}

void WorkspaceManager::UpdateShelfVisibility() {
  if (shelf_)
    shelf_->UpdateVisibilityState();
}

Workspace* WorkspaceManager::FindBy(Window* window) const {
  while (window) {
    Workspace* workspace = window->GetProperty(kWorkspaceKey);
    if (workspace)
      return workspace;
    window = window->parent();
  }
  return NULL;
}

void WorkspaceManager::SetActiveWorkspace(Workspace* workspace,
                                          SwitchReason reason) {
  DCHECK(workspace);
  if (active_workspace_ == workspace)
    return;

  pending_workspaces_.erase(workspace);

  // Adjust the z-order. No need to adjust the z-order for the desktop since
  // it always stays at the bottom.
  if (workspace != desktop_workspace() &&
      FindWorkspace(workspace) == workspaces_.end()) {
    contents_window_->StackChildAbove(workspace->window(),
                                      workspaces_.back()->window());
    workspaces_.push_back(workspace);
  }

  Workspace* last_active = active_workspace_;
  active_workspace_ = workspace;

  // The display work-area may have changed while |workspace| was not the active
  // workspace. Give it a chance to adjust its state for the new work-area.
  active_workspace_->workspace_layout_manager()->
      OnDisplayWorkAreaInsetsChanged();

  contents_window_->StackChildAtTop(last_active->window());

  HideWorkspace(last_active, reason);
  ShowWorkspace(workspace, last_active, reason);

  UpdateShelfVisibility();

  // Showing or hiding a workspace may change the "solo window" status of
  // a window, requiring the header to be updated.
  FramePainter::UpdateSoloWindowHeader(contents_window_->GetRootWindow());
}

WorkspaceManager::Workspaces::iterator
WorkspaceManager::FindWorkspace(Workspace* workspace)  {
  return std::find(workspaces_.begin(), workspaces_.end(), workspace);
}

Workspace* WorkspaceManager::CreateWorkspaceForTest() {
  return new Workspace(this, contents_window_);
}

void WorkspaceManager::MoveWorkspaceToPendingOrDelete(
    Workspace* workspace,
    Window* stack_beneath,
    SwitchReason reason) {
  // We're all ready moving windows.
  if (in_move_)
    return;

  DCHECK_NE(desktop_workspace(), workspace);

  if (workspace == active_workspace_)
    SelectNextWorkspace(reason);

  base::AutoReset<bool> setter(&in_move_, true);

  MoveChildrenToDesktop(workspace->window(), stack_beneath);

  {
    Workspaces::iterator workspace_i(FindWorkspace(workspace));
    if (workspace_i != workspaces_.end())
      workspaces_.erase(workspace_i);
  }

  if (workspace->window()->children().empty()) {
    pending_workspaces_.erase(workspace);
    ScheduleDelete(workspace);
  } else {
    pending_workspaces_.insert(workspace);
  }
}

void WorkspaceManager::MoveChildrenToDesktop(aura::Window* window,
                                             aura::Window* stack_beneath) {
  // Build the list of windows to move. Exclude fullscreen and windows with
  // transient parents.
  Window::Windows to_move;
  for (size_t i = 0; i < window->children().size(); ++i) {
    Window* child = window->children()[i];
    if (!child->transient_parent() && !wm::IsWindowFullscreen(child)) {
      to_move.push_back(child);
    }
  }
  // Move the windows, but make sure the window is still a child of |window|
  // (moving may cascade and cause other windows to move).
  for (size_t i = 0; i < to_move.size(); ++i) {
    if (std::find(window->children().begin(), window->children().end(),
                  to_move[i]) != window->children().end()) {
      ReparentWindow(to_move[i], desktop_workspace()->window(),
                     stack_beneath);
    }
  }
}

void WorkspaceManager::SelectNextWorkspace(SwitchReason reason) {
  DCHECK_NE(active_workspace_, desktop_workspace());

  Workspaces::const_iterator workspace_i(FindWorkspace(active_workspace_));
  Workspaces::const_iterator next_workspace_i(workspace_i + 1);
  if (next_workspace_i != workspaces_.end())
    SetActiveWorkspace(*next_workspace_i, reason);
  else
    SetActiveWorkspace(*(workspace_i - 1), reason);
}

void WorkspaceManager::ScheduleDelete(Workspace* workspace) {
  to_delete_.insert(workspace);
  delete_timer_.Stop();
  delete_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                      &WorkspaceManager::ProcessDeletion);
}

void WorkspaceManager::FadeDesktop(aura::Window* window,
                                   base::TimeDelta duration) {
  if (views::corewm::WindowAnimationsDisabled(NULL) ||
      ui::ScopedAnimationDurationScaleMode::duration_scale_mode() ==
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION)
    return;

  base::AutoReset<bool> reseter(&creating_fade_, true);
  DesktopBackgroundFadeController::Direction direction;
  aura::Window* parent = NULL;
  aura::Window* stack_above = NULL;
  if (active_workspace_ == desktop_workspace()) {
    direction = DesktopBackgroundFadeController::FADE_IN;
    parent = desktop_workspace()->window();
    stack_above = window;
  } else {
    direction = DesktopBackgroundFadeController::FADE_OUT;
    parent = contents_window_;
    stack_above = desktop_workspace()->window();
    DCHECK_EQ(kCrossFadeSwitchTimeMS,
              static_cast<int>(duration.InMilliseconds()));
    duration = base::TimeDelta::FromMilliseconds(kCrossFadeSwitchTimeMS);
  }
  desktop_fade_controller_.reset(
      new DesktopBackgroundFadeController(
          parent, stack_above, duration, direction));
}

void WorkspaceManager::ShowWorkspace(
    Workspace* workspace,
    Workspace* last_active,
    SwitchReason reason) const {
  WorkspaceAnimationDetails details;
  details.direction =
      (last_active == desktop_workspace() || reason == SWITCH_INITIAL) ?
      WORKSPACE_ANIMATE_DOWN : WORKSPACE_ANIMATE_UP;

  switch (reason) {
    case SWITCH_WINDOW_MADE_ACTIVE:
    case SWITCH_TRACKED_BY_WORKSPACE_CHANGED:
      details.animate = details.animate_scale = true;
      details.animate_opacity = last_active == desktop_workspace();
      break;

    case SWITCH_INITIAL:
      details.animate = details.animate_opacity = details.animate_scale = true;
      details.pause_time_ms = kInitialPauseTimeMS;
      break;

    // Remaining cases require no animation.
    default:
      break;
  }
  ash::internal::ShowWorkspace(workspace->window(), details);
}

void WorkspaceManager::HideWorkspace(
    Workspace* workspace,
    SwitchReason reason) const {
  WorkspaceAnimationDetails details;
  details.direction = active_workspace_ == desktop_workspace() ?
      WORKSPACE_ANIMATE_UP : WORKSPACE_ANIMATE_DOWN;
  switch (reason) {
    case SWITCH_WINDOW_MADE_ACTIVE:
    case SWITCH_TRACKED_BY_WORKSPACE_CHANGED:
      details.animate_opacity =
          ((active_workspace_ == desktop_workspace() ||
            workspace != desktop_workspace()));
      details.animate_scale = true;
      details.animate = true;
      break;

    // Remaining cases require no animation.
    default:
      break;
  }
  ash::internal::HideWorkspace(workspace->window(), details);
}

void WorkspaceManager::ProcessDeletion() {
  std::set<Workspace*> to_delete;
  to_delete.swap(to_delete_);
  for (std::set<Workspace*>::iterator i = to_delete.begin();
       i != to_delete.end(); ++i) {
    Workspace* workspace = *i;
    if (workspace->window()->layer()->children().empty()) {
      delete workspace->ReleaseWindow();
      delete workspace;
    } else {
      to_delete_.insert(workspace);
    }
  }
  if (!to_delete_.empty()) {
    delete_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                        &WorkspaceManager::ProcessDeletion);
  }
}

void WorkspaceManager::OnWindowAddedToWorkspace(Workspace* workspace,
                                                Window* child) {
  child->SetProperty(kWorkspaceKey, workspace);
  // Don't make changes to window parenting as the right parent was chosen
  // by way of GetParentForNewWindow() or we explicitly moved the window
  // to the workspace.
  if (workspace == active_workspace_) {
    UpdateShelfVisibility();
    FramePainter::UpdateSoloWindowHeader(child->GetRootWindow());
  }

  RearrangeVisibleWindowOnShow(child);
}

void WorkspaceManager::OnWillRemoveWindowFromWorkspace(Workspace* workspace,
                                                       Window* child) {
  if (child->TargetVisibility())
    RearrangeVisibleWindowOnHideOrRemove(child);
  child->ClearProperty(kWorkspaceKey);
}

void WorkspaceManager::OnWindowRemovedFromWorkspace(Workspace* workspace,
                                                    Window* child) {
  UpdateShelfVisibility();
}

void WorkspaceManager::OnWorkspaceChildWindowVisibilityChanged(
    Workspace* workspace,
    Window* child) {
  if (child->TargetVisibility())
    RearrangeVisibleWindowOnShow(child);
  else
    RearrangeVisibleWindowOnHideOrRemove(child);
  if (workspace == active_workspace_) {
    UpdateShelfVisibility();
    FramePainter::UpdateSoloWindowHeader(child->GetRootWindow());
  }
}

void WorkspaceManager::OnWorkspaceWindowChildBoundsChanged(
    Workspace* workspace,
    Window* child) {
  if (workspace == active_workspace_)
    UpdateShelfVisibility();
}

void WorkspaceManager::OnWorkspaceWindowShowStateChanged(
    Workspace* workspace,
    Window* child,
    ui::WindowShowState last_show_state) {
  // |child| better still be in |workspace| else things have gone wrong.
  DCHECK_EQ(workspace, child->GetProperty(kWorkspaceKey));

  if (!wm::IsWindowMinimized(child) && workspace != desktop_workspace()) {
    {
      base::AutoReset<bool> setter(&in_move_, true);
      ReparentWindow(child, desktop_workspace()->window(), NULL);
    }
    SetActiveWorkspace(desktop_workspace(), SWITCH_MAXIMIZED_OR_RESTORED);
    MoveWorkspaceToPendingOrDelete(workspace,
                                   child,
                                   SWITCH_MAXIMIZED_OR_RESTORED);
  }
  UpdateShelfVisibility();
}

void WorkspaceManager::OnTrackedByWorkspaceChanged(Workspace* workspace,
                                                   aura::Window* window) {
  if (workspace == active_workspace_)
    return;

  // If the window is active we need to make sure the destination Workspace
  // window is showing. Otherwise the window will be parented to a hidden window
  // and lose activation.
  const bool is_active = wm::IsActiveWindow(window);
  if (is_active)
    workspace->window()->Show();
  ReparentWindow(window, workspace->window(), NULL);
  if (is_active)
    SetActiveWorkspace(workspace, SWITCH_TRACKED_BY_WORKSPACE_CHANGED);
}

}  // namespace internal
}  // namespace ash
