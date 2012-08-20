// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_MANAGER2_H_
#define ASH_WM_WORKSPACE_WORKSPACE_MANAGER2_H_

#include <set>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/workspace/base_workspace_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
class Rect;
}

namespace ash {
namespace internal {

class ShelfLayoutManager;
class WorkspaceManagerTest2;
class Workspace2;

// WorkspaceManager manages multiple workspaces in the desktop. Workspaces are
// implicitly created as windows are maximized (or made fullscreen), and
// destroyed when maximized windows are closed or restored. There is always one
// workspace for the desktop.
// Internally WorkspaceManager2 creates a Window for each Workspace. As windows
// are maximized and restored they are reparented to the right Window.
class ASH_EXPORT WorkspaceManager2 : public BaseWorkspaceManager {
 public:
  explicit WorkspaceManager2(aura::Window* viewport);
  virtual ~WorkspaceManager2();

  // Returns true if |window| is considered maximized and should exist in its
  // own workspace.
  static bool IsMaximized(aura::Window* window);

  // Returns true if |window| is minimized and will restore to a maximized
  // window.
  static bool WillRestoreMaximized(aura::Window* window);

  // BaseWorkspaceManager2 overrides:
  virtual bool IsInMaximizedMode() const OVERRIDE;
  virtual void SetGridSize(int size) OVERRIDE;
  virtual int GetGridSize() const OVERRIDE;
  virtual WorkspaceWindowState GetWindowState() const OVERRIDE;
  virtual void SetShelf(ShelfLayoutManager* shelf) OVERRIDE;
  virtual void SetActiveWorkspaceByWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetParentForNewWindow(aura::Window* window) OVERRIDE;

 private:
  friend class WorkspaceManager2Test;

  class LayoutManager;
  class WorkspaceLayoutManager;

  typedef std::vector<Workspace2*> Workspaces;

  // Updates the visibility and whether any windows overlap the shelf.
  void UpdateShelfVisibility();

  // Returns the workspace that contains |window|.
  Workspace2* FindBy(aura::Window* window) const;

  // Sets the active workspace.
  void SetActiveWorkspace(Workspace2* workspace);

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
                                      aura::Window* stack_beneath);

  // Selects the next workspace.
  void SelectNextWorkspace();

  // These methods are forwarded from the LayoutManager installed on the
  // Workspace's window.
  void OnWindowAddedToWorkspace(Workspace2* workspace, aura::Window* child);
  void OnWindowRemovedFromWorkspace(Workspace2* workspace, aura::Window* child);
  void OnWorkspaceChildWindowVisibilityChanged(Workspace2* workspace,
                                               aura::Window* child);
  void OnWorkspaceWindowChildBoundsChanged(Workspace2* workspace,
                                           aura::Window* child);
  void OnWorkspaceWindowShowStateChanged(Workspace2* workspace,
                                         aura::Window* child,
                                         ui::WindowShowState last_show_state);

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

  // See description above setter.
  int grid_size_;

  // Owned by the Shell. May be NULL.
  ShelfLayoutManager* shelf_;

  // Whether or not we're in MoveWorkspaceToPendingOrDelete(). As
  // MoveWorkspaceToPendingOrDelete() may trigger another call to
  // MoveWorkspaceToPendingOrDelete() we use this to avoid doing anything if
  // already in MoveWorkspaceToPendingOrDelete().
  bool in_move_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManager2);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_MANAGER2_H_
