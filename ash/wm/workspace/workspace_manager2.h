// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_MANAGER2_H_
#define ASH_WM_WORKSPACE_WORKSPACE_MANAGER2_H_

#include <set>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {
class Layer;
}

namespace ash {
namespace internal {

class DesktopBackgroundFadeController;
class ShelfLayoutManager;
class WorkspaceLayoutManager2;
class WorkspaceManagerTest2;
class Workspace2;

// WorkspaceManager manages multiple workspaces in the desktop. Workspaces are
// implicitly created as windows are maximized (or made fullscreen), and
// destroyed when maximized windows are closed or restored. There is always one
// workspace for the desktop.
// Internally WorkspaceManager2 creates a Window for each Workspace. As windows
// are maximized and restored they are reparented to the right Window.
class ASH_EXPORT WorkspaceManager2 : public ash::ShellObserver {
 public:
  explicit WorkspaceManager2(aura::Window* viewport);
  virtual ~WorkspaceManager2();

  // Returns true if |window| is considered maximized and should exist in its
  // own workspace.
  static bool IsMaximized(aura::Window* window);
  static bool IsMaximizedState(ui::WindowShowState state);

  // Returns true if |window| is minimized and will restore to a maximized
  // window.
  static bool WillRestoreMaximized(aura::Window* window);

  // Returns the current window state.
  WorkspaceWindowState GetWindowState() const;

  void SetShelf(ShelfLayoutManager* shelf);

  // Activates the workspace containing |window|. Does nothing if |window| is
  // NULL or not contained in a workspace.
  void SetActiveWorkspaceByWindow(aura::Window* window);

  // Returns the parent for |window|. This is invoked from StackingController
  // when a new Window is being added.
  aura::Window* GetParentForNewWindow(aura::Window* window);

  // Starts the animation that occurs on first login.
  void DoInitialAnimation();

  // ShellObserver overrides:
  virtual void OnAppTerminating() OVERRIDE;

 private:
  friend class WorkspaceLayoutManager2;
  friend class WorkspaceManager2Test;

  class LayoutManagerImpl;

  typedef std::vector<Workspace2*> Workspaces;

  // Reason for the workspace switch. Used to determine the characterstics of
  // the animation.
  enum SwitchReason {
    SWITCH_WINDOW_MADE_ACTIVE,
    SWITCH_WINDOW_REMOVED,
    SWITCH_VISIBILITY_CHANGED,
    SWITCH_MINIMIZED,
    SWITCH_MAXIMIZED_OR_RESTORED,
    SWITCH_TRACKED_BY_WORKSPACE_CHANGED,

    // Switch as the result of DoInitialAnimation(). This isn't a real switch,
    // rather we run the animations as if a switch occurred.
    SWITCH_INITIAL,

    // Edge case. See comment in OnWorkspaceWindowShowStateChanged().  Don't
    // make other types randomly use this!
    SWITCH_OTHER,
  };

  // Updates the visibility and whether any windows overlap the shelf.
  void UpdateShelfVisibility();

  // Returns the workspace that contains |window|.
  Workspace2* FindBy(aura::Window* window) const;

  // Sets the active workspace.
  void SetActiveWorkspace(Workspace2* workspace,
                          SwitchReason reason,
                          base::TimeDelta duration);

  // Returns the bounds of the work area.
  gfx::Rect GetWorkAreaBounds() const;

  // Returns an iterator into |workspaces_| for |workspace|.
  Workspaces::iterator FindWorkspace(Workspace2* workspace);

  Workspace2* desktop_workspace() { return workspaces_[0]; }
  const Workspace2* desktop_workspace() const { return workspaces_[0]; }

  // Creates a new workspace. The Workspace is not added to anything and is
  // owned by the caller.
  Workspace2* CreateWorkspace(bool maximized);

  // Moves all the non-maximized child windows of |workspace| to the desktop
  // stacked beneath |stack_beneath| (if non-NULL). After moving child windows
  // if |workspace| contains no children it is deleted, otherwise it it moved to
  // |pending_workspaces_|.
  void MoveWorkspaceToPendingOrDelete(Workspace2* workspace,
                                      aura::Window* stack_beneath,
                                      SwitchReason reason);

  // Moves the children of |window| to the desktop. This excludes certain
  // windows. If |stack_beneath| is non-NULL the windows are stacked beneath it.
  void MoveChildrenToDesktop(aura::Window* window, aura::Window* stack_beneath);

  // Selects the next workspace.
  void SelectNextWorkspace(SwitchReason reason);

  // Schedules |workspace| for deletion when it no longer contains any layers.
  // See comments above |to_delete_| as to why we do this.
  void ScheduleDelete(Workspace2* workspace);

  // Deletes any workspaces scheduled via ScheduleDelete() that don't contain
  // any layers.
  void ProcessDeletion();

  // Sets |unminimizing_workspace_| to |workspace|.
  void SetUnminimizingWorkspace(Workspace2* workspace);

  // Fades the desktop. This is only used when maximizing or restoring a
  // window. The actual fade is handled by
  // DesktopBackgroundFadeController. |window| is used when restoring and
  // indicates the window to stack the DesktopBackgroundFadeController's window
  // above.
  void FadeDesktop(aura::Window* window, base::TimeDelta duration);

  // Shows or hides the desktop Window |window|.
  void ShowOrHideDesktopBackground(aura::Window* window,
                                   SwitchReason reason,
                                   base::TimeDelta duration,
                                   bool show) const;

  // Shows/hides |workspace| animating as necessary.
  void ShowWorkspace(Workspace2* workspace,
                     Workspace2* last_active,
                     SwitchReason reason) const;
  void HideWorkspace(Workspace2* workspace,
                     SwitchReason reason,
                     bool is_unminimizing_maximized_window) const;

  // These methods are forwarded from the LayoutManager installed on the
  // Workspace's window.
  void OnWindowAddedToWorkspace(Workspace2* workspace, aura::Window* child);
  void OnWillRemoveWindowFromWorkspace(Workspace2* workspace,
                                       aura::Window* child);
  void OnWindowRemovedFromWorkspace(Workspace2* workspace, aura::Window* child);
  void OnWorkspaceChildWindowVisibilityChanged(Workspace2* workspace,
                                               aura::Window* child);
  void OnWorkspaceWindowChildBoundsChanged(Workspace2* workspace,
                                           aura::Window* child);
  void OnWorkspaceWindowShowStateChanged(Workspace2* workspace,
                                         aura::Window* child,
                                         ui::WindowShowState last_show_state,
                                         ui::Layer* old_layer);
  void OnTrackedByWorkspaceChanged(Workspace2* workspace,
                                   aura::Window* window);

  aura::Window* contents_view_;

  Workspace2* active_workspace_;

  // The set of active workspaces. There is always at least one in this stack,
  // which identifies the desktop.
  Workspaces workspaces_;

  // The set of workspaces not currently active. Workspaces ended up here in
  // two scenarios:
  // . Prior to adding a window a new worskpace is created for it. The
  //   Workspace is added to this set.
  // . When the maximized window is minimized the workspace is added here.
  // Once any window in the workspace is activated the workspace is moved to
  // |workspaces_|.
  std::set<Workspace2*> pending_workspaces_;

  // Owned by the Shell. May be NULL.
  ShelfLayoutManager* shelf_;

  // Whether or not we're in MoveWorkspaceToPendingOrDelete(). As
  // MoveWorkspaceToPendingOrDelete() may trigger another call to
  // MoveWorkspaceToPendingOrDelete() we use this to avoid doing anything if
  // already in MoveWorkspaceToPendingOrDelete().
  bool in_move_;

  // Ideally we would delete workspaces when not needed. Unfortunately doing so
  // would effectively cancel animations. Instead when a workspace is no longer
  // needed we add it here and start a timer. When the timer fires any windows
  // no longer contain layers are deleted.
  std::set<Workspace2*> to_delete_;
  base::OneShotTimer<WorkspaceManager2> delete_timer_;

  // See comments in SetUnminimizingWorkspace() for details.
  base::WeakPtrFactory<WorkspaceManager2> clear_unminimizing_workspace_factory_;

  // See comments in SetUnminimizingWorkspace() for details.
  Workspace2* unminimizing_workspace_;

  // Set to true if the app is terminating. If true we don't animate the
  // background, otherwise it can get stuck in the fading position when chrome
  // exits (as the last frame we draw before exiting is a frame from the
  // animation).
  bool app_terminating_;

  scoped_ptr<DesktopBackgroundFadeController> desktop_fade_controller_;

  // Set to true while in the process of creating a
  // DesktopBackgroundFadeController.
  bool creating_fade_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManager2);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_MANAGER2_H_
