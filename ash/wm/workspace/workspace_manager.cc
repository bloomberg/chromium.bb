// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager.h"

#include <algorithm>
#include <functional>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/base_layout_manager.h"
#include "ash/wm/frame_painter.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
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
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::Workspace*);
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, ui::WindowShowState);

using aura::Window;

namespace ash {
namespace internal {

DEFINE_WINDOW_PROPERTY_KEY(Workspace*, kWorkspaceKey, NULL);

namespace {

// Duration for fading out the desktop background when maximizing.
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
// |WorkspaceManager::contents_view_|).
class WorkspaceManager::LayoutManagerImpl : public BaseLayoutManager {
 public:
  explicit LayoutManagerImpl(WorkspaceManager* workspace_manager)
      : BaseLayoutManager(workspace_manager->contents_view_->GetRootWindow()),
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
  aura::Window* window() { return workspace_manager_->contents_view_; }

  WorkspaceManager* workspace_manager_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerImpl);
};

// WorkspaceManager -----------------------------------------------------------

WorkspaceManager::WorkspaceManager(Window* contents_view)
    : contents_view_(contents_view),
      active_workspace_(NULL),
      shelf_(NULL),
      in_move_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          clear_unminimizing_workspace_factory_(this)),
      unminimizing_workspace_(NULL),
      app_terminating_(false),
      creating_fade_(false) {
  // Clobber any existing event filter.
  contents_view->SetEventFilter(NULL);
  // |contents_view| takes ownership of LayoutManagerImpl.
  contents_view->SetLayoutManager(new LayoutManagerImpl(this));
  active_workspace_ = CreateWorkspace(false);
  workspaces_.push_back(active_workspace_);
  active_workspace_->window()->Show();
  Shell::GetInstance()->AddShellObserver(this);
}

WorkspaceManager::~WorkspaceManager() {
  Shell::GetInstance()->RemoveShellObserver(this);
  // Release the windows, they'll be destroyed when |contents_view_| is
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

// static
bool WorkspaceManager::IsMaximized(Window* window) {
  return IsMaximizedState(window->GetProperty(aura::client::kShowStateKey));
}

// static
bool WorkspaceManager::IsMaximizedState(ui::WindowShowState state) {
  return state == ui::SHOW_STATE_MAXIMIZED ||
      state == ui::SHOW_STATE_FULLSCREEN;
}

// static
bool WorkspaceManager::WillRestoreMaximized(Window* window) {
  return wm::IsWindowMinimized(window) &&
      IsMaximizedState(window->GetProperty(internal::kRestoreShowStateKey));
}

WorkspaceWindowState WorkspaceManager::GetWindowState() const {
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

void WorkspaceManager::SetShelf(ShelfLayoutManager* shelf) {
  shelf_ = shelf;
}

void WorkspaceManager::SetActiveWorkspaceByWindow(Window* window) {
  Workspace* workspace = FindBy(window);
  if (!workspace)
    return;

  if (workspace != active_workspace_) {
    // A window is being made active. In the following cases we reparent to
    // the active desktop:
    // . The window is not tracked by workspace code. This is used for tab
    //   dragging. Since tab dragging needs to happen in the active workspace we
    //   have to reparent the window (otherwise the window you dragged the tab
    //   out of would disappear since the workspace changed). Since this case is
    //   only transiently used (property reset on input release) we don't worry
    //   about window state. In fact we can't consider window state here as we
    //   have to allow dragging of a maximized window to work in this case.
    // . The window persists across all workspaces. For example, the task
    //   manager is in the desktop worskpace and the current workspace is
    //   maximized. If we swapped to the desktop you would lose context. Instead
    //   we reparent.  The exception to this is if the window is maximized (it
    //   needs its own workspace then) or we're in the process of maximizing. If
    //   we're in the process of maximizing the window needs its own workspace.
    if (!GetTrackedByWorkspace(window) ||
        (GetPersistsAcrossAllWorkspaces(window) && !IsMaximized(window) &&
         !(wm::IsWindowMinimized(window) && WillRestoreMaximized(window)))) {
      ReparentWindow(window, active_workspace_->window(), NULL);
    } else {
      SetActiveWorkspace(workspace, SWITCH_WINDOW_MADE_ACTIVE,
                         base::TimeDelta());
    }
  }
  if (workspace->is_maximized() && IsMaximized(window)) {
    // Clicking on the maximized window in a maximized workspace. Force all
    // other windows to drop to the desktop.
    MoveChildrenToDesktop(workspace->window(), NULL);
  }
}

Window* WorkspaceManager::GetActiveWorkspaceWindow() {
  return active_workspace_->window();
}

Window* WorkspaceManager::GetParentForNewWindow(Window* window) {
  // Try to put windows with transient parents in the same workspace as their
  // transient parent.
  if (window->transient_parent() && !IsMaximized(window)) {
    Workspace* workspace = FindBy(window->transient_parent());
    if (workspace)
      return workspace->window();
    // Fall through to normal logic.
  }

  if (!GetTrackedByWorkspace(window))
    return active_workspace_->window();

  if (IsMaximized(window)) {
    // Wait for the window to be made active before showing the workspace.
    Workspace* workspace = CreateWorkspace(true);
    pending_workspaces_.insert(workspace);
    return workspace->window();
  }

  if (!GetTrackedByWorkspace(window) || GetPersistsAcrossAllWorkspaces(window))
    return active_workspace_->window();

  return desktop_workspace()->window();
}

bool WorkspaceManager::CycleToWorkspace(CycleDirection direction) {
  aura::Window* active_window = wm::GetActiveWindow();
  if (!active_workspace_->window()->Contains(active_window))
    active_window = NULL;

  Workspaces::const_iterator workspace_i(FindWorkspace(active_workspace_));
  int workspace_offset = 0;
  if (direction == CYCLE_PREVIOUS) {
    workspace_offset = 1;
    if (workspace_i == workspaces_.end() - 1)
      return false;
  } else {
    workspace_offset = -1;
    if (workspace_i == workspaces_.begin())
      return false;
  }

  Workspaces::const_iterator next_workspace_i(workspace_i + workspace_offset);
  SetActiveWorkspace(*next_workspace_i, SWITCH_OTHER, base::TimeDelta());

  // The activation controller will pick a window from the just activated
  // workspace to activate as a result of DeactivateWindow().
  if (active_window)
    wm::DeactivateWindow(active_window);
  return true;
}

void WorkspaceManager::DoInitialAnimation() {
  if (active_workspace_->is_maximized()) {
    RootWindowController* root_controller = GetRootWindowController(
        contents_view_->GetRootWindow());
    if (root_controller) {
      aura::Window* background = root_controller->GetContainer(
          kShellWindowId_DesktopBackgroundContainer);
      background->Show();
      ShowOrHideDesktopBackground(background, SWITCH_INITIAL,
                                  base::TimeDelta(), false);
    }
  }
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
                                          SwitchReason reason,
                                          base::TimeDelta duration) {
  DCHECK(workspace);
  if (active_workspace_ == workspace)
    return;

  pending_workspaces_.erase(workspace);

  // Adjust the z-order. No need to adjust the z-order for the desktop since
  // it always stays at the bottom.
  if (workspace != desktop_workspace() &&
      FindWorkspace(workspace) == workspaces_.end()) {
    contents_view_->StackChildAbove(workspace->window(),
                                    workspaces_.back()->window());
    workspaces_.push_back(workspace);
  }

  Workspace* last_active = active_workspace_;
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

  UpdateShelfVisibility();

  // NOTE: duration supplied to this method is only used for desktop background.
  HideWorkspace(last_active, reason, is_unminimizing_maximized_window);
  ShowWorkspace(workspace, last_active, reason);

  RootWindowController* root_controller = GetRootWindowController(
      contents_view_->GetRootWindow());
  if (root_controller) {
    aura::Window* background = root_controller->GetContainer(
        kShellWindowId_DesktopBackgroundContainer);
    if (last_active == desktop_workspace()) {
      ShowOrHideDesktopBackground(background, reason, duration, false);
    } else if (active_workspace_ == desktop_workspace() && !app_terminating_) {
      ShowOrHideDesktopBackground(background, reason, duration, true);
    }
  }

  // Showing or hiding a workspace may change the "solo window" status of
  // a window, requiring the header to be updated.
  FramePainter::UpdateSoloWindowHeader(contents_view_->GetRootWindow());
}

WorkspaceManager::Workspaces::iterator
WorkspaceManager::FindWorkspace(Workspace* workspace)  {
  return std::find(workspaces_.begin(), workspaces_.end(), workspace);
}

Workspace* WorkspaceManager::CreateWorkspace(bool maximized) {
  return new Workspace(this, contents_view_, maximized);
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
    if (workspace == unminimizing_workspace_)
      unminimizing_workspace_ = NULL;
    pending_workspaces_.erase(workspace);
    ScheduleDelete(workspace);
  } else {
    pending_workspaces_.insert(workspace);
  }
}

void WorkspaceManager::MoveChildrenToDesktop(aura::Window* window,
                                             aura::Window* stack_beneath) {
  // Build the list of windows to move. Exclude maximized/fullscreen and windows
  // with transient parents.
  Window::Windows to_move;
  for (size_t i = 0; i < window->children().size(); ++i) {
    Window* child = window->children()[i];
    if (!child->transient_parent() && !IsMaximized(child) &&
        !WillRestoreMaximized(child)) {
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
    SetActiveWorkspace(*next_workspace_i, reason, base::TimeDelta());
  else
    SetActiveWorkspace(*(workspace_i - 1), reason, base::TimeDelta());
}

void WorkspaceManager::ScheduleDelete(Workspace* workspace) {
  to_delete_.insert(workspace);
  delete_timer_.Stop();
  delete_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                      &WorkspaceManager::ProcessDeletion);
}

void WorkspaceManager::SetUnminimizingWorkspace(Workspace* workspace) {
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
        base::Bind(&WorkspaceManager::SetUnminimizingWorkspace,
                   clear_unminimizing_workspace_factory_.GetWeakPtr(),
                   static_cast<Workspace*>(NULL)));
  }
}

void WorkspaceManager::FadeDesktop(aura::Window* window,
                                   base::TimeDelta duration) {
  if (views::corewm::WindowAnimationsDisabled(NULL) ||
      ui::LayerAnimator::disable_animations_for_test())
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
    parent = contents_view_;
    stack_above = desktop_workspace()->window();
    DCHECK_EQ(kCrossFadeSwitchTimeMS, (int)duration.InMilliseconds());
    duration = base::TimeDelta::FromMilliseconds(kCrossFadeSwitchTimeMS);
  }
  desktop_fade_controller_.reset(
      new DesktopBackgroundFadeController(
          parent, stack_above, duration, direction));
}

void WorkspaceManager::ShowOrHideDesktopBackground(
    aura::Window* window,
    SwitchReason reason,
    base::TimeDelta duration,
    bool show) const {
  WorkspaceAnimationDetails details;
  details.animate = true;
  details.direction = show ? WORKSPACE_ANIMATE_UP : WORKSPACE_ANIMATE_DOWN;
  details.animate_scale = reason != SWITCH_MAXIMIZED_OR_RESTORED;
  details.duration = duration;
  if (reason == SWITCH_INITIAL)
    details.pause_time_ms = kInitialPauseTimeMS;
  if (show)
    ash::internal::ShowWorkspace(window, details);
  else
    ash::internal::HideWorkspace(window, details);
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
    case SWITCH_WINDOW_REMOVED:
    case SWITCH_VISIBILITY_CHANGED:
    case SWITCH_MINIMIZED:
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
    SwitchReason reason,
    bool is_unminimizing_maximized_window) const {
  WorkspaceAnimationDetails details;
  details.direction = active_workspace_ == desktop_workspace() ?
      WORKSPACE_ANIMATE_UP : WORKSPACE_ANIMATE_DOWN;
  switch (reason) {
    case SWITCH_WINDOW_MADE_ACTIVE:
    case SWITCH_TRACKED_BY_WORKSPACE_CHANGED:
      details.animate_opacity =
          ((active_workspace_ == desktop_workspace() ||
            workspace != desktop_workspace()) &&
           !is_unminimizing_maximized_window);
      details.animate_scale = true;
      details.animate = true;
      break;

    case SWITCH_VISIBILITY_CHANGED:
      // The window is most likely closing. Make the workspace visible for the
      // duration of the switch so that the close animation is visible.
      details.animate = true;
      details.animate_scale = true;
      break;

    case SWITCH_MAXIMIZED_OR_RESTORED:
      if (active_workspace_->is_maximized()) {
        // Delay the hide until the animation is done.
        details.duration =
            base::TimeDelta::FromMilliseconds(kCrossFadeSwitchTimeMS);
        details.animate = true;
      }
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
  // Do nothing (other than updating shelf visibility) as the right parent was
  // chosen by way of GetParentForNewWindow() or we explicitly moved the window
  // to the workspace.
  if (workspace == active_workspace_)
    UpdateShelfVisibility();

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
  if (workspace->ShouldMoveToPending())
    MoveWorkspaceToPendingOrDelete(workspace, NULL, SWITCH_WINDOW_REMOVED);
}

void WorkspaceManager::OnWorkspaceChildWindowVisibilityChanged(
    Workspace* workspace,
    Window* child) {
  if (workspace->ShouldMoveToPending()) {
    MoveWorkspaceToPendingOrDelete(workspace, NULL, SWITCH_VISIBILITY_CHANGED);
  } else {
    if (child->TargetVisibility())
      RearrangeVisibleWindowOnShow(child);
    else
      RearrangeVisibleWindowOnHideOrRemove(child);
    if (workspace == active_workspace_)
      UpdateShelfVisibility();
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
    ui::WindowShowState last_show_state,
    ui::Layer* old_layer) {
  // |child| better still be in |workspace| else things have gone wrong.
  DCHECK_EQ(workspace, child->GetProperty(kWorkspaceKey));
  if (wm::IsWindowMinimized(child)) {
    if (workspace->ShouldMoveToPending())
      MoveWorkspaceToPendingOrDelete(workspace, NULL, SWITCH_MINIMIZED);
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
    Workspace* new_workspace = NULL;
    const int max_count = workspace->GetNumMaximizedWindows();
    base::TimeDelta duration = old_layer && !IsMaximized(child) ?
            GetCrossFadeDuration(old_layer->bounds(), child->bounds()) :
            base::TimeDelta::FromMilliseconds(kCrossFadeSwitchTimeMS);
    if (max_count == 0) {
      if (workspace != desktop_workspace()) {
        {
          base::AutoReset<bool> setter(&in_move_, true);
          ReparentWindow(child, desktop_workspace()->window(), NULL);
        }
        DCHECK(!is_active || old_layer);
        new_workspace = desktop_workspace();
        SetActiveWorkspace(new_workspace, SWITCH_MAXIMIZED_OR_RESTORED,
                           duration);
        MoveWorkspaceToPendingOrDelete(workspace, child,
                                       SWITCH_MAXIMIZED_OR_RESTORED);
        if (FindWorkspace(workspace) == workspaces_.end())
          workspace = NULL;
      }
    } else if ((max_count == 1 && workspace == desktop_workspace()) ||
               max_count > 1) {
      new_workspace = CreateWorkspace(true);
      pending_workspaces_.insert(new_workspace);
      ReparentWindow(child, new_workspace->window(), NULL);
    }
    if (is_active && new_workspace) {
      // |old_layer| may be NULL if as part of processing
      // WorkspaceLayoutManager::OnWindowPropertyChanged() the window is made
      // active.
      if (old_layer) {
        SetActiveWorkspace(new_workspace, SWITCH_MAXIMIZED_OR_RESTORED,
                           duration);
        CrossFadeWindowBetweenWorkspaces(new_workspace->window(), child,
                                         old_layer);
        if (workspace == desktop_workspace() ||
            new_workspace == desktop_workspace()) {
          FadeDesktop(child, duration);
        }
      } else {
        SetActiveWorkspace(new_workspace, SWITCH_OTHER, base::TimeDelta());
      }
    } else {
      if (last_show_state == ui::SHOW_STATE_MINIMIZED)
        SetUnminimizingWorkspace(new_workspace ? new_workspace : workspace);
      DCHECK(!old_layer);
    }
  }
  UpdateShelfVisibility();
}

void WorkspaceManager::OnTrackedByWorkspaceChanged(Workspace* workspace,
                                                   aura::Window* window) {
  Workspace* new_workspace = NULL;
  if (IsMaximized(window)) {
    new_workspace = CreateWorkspace(true);
    pending_workspaces_.insert(new_workspace);
  } else if (workspace->is_maximized()) {
    new_workspace = desktop_workspace();
  } else {
    return;
  }
  // If the window is active we need to make sure the destination Workspace
  // window is showing. Otherwise the window will be parented to a hidden window
  // and lose activation.
  const bool is_active = wm::IsActiveWindow(window);
  if (is_active)
    new_workspace->window()->Show();
  ReparentWindow(window, new_workspace->window(), NULL);
  if (is_active) {
    SetActiveWorkspace(new_workspace, SWITCH_TRACKED_BY_WORKSPACE_CHANGED,
                       base::TimeDelta());
  }
}

}  // namespace internal
}  // namespace ash
