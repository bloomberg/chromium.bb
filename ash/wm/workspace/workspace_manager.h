// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_MANAGER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_MANAGER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/workspace/base_workspace_manager.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/size.h"

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
class WorkspaceManagerTest;

// WorkspaceManager manages multiple workspaces in the desktop.
class ASH_EXPORT WorkspaceManager : public BaseWorkspaceManager {
 public:
  explicit WorkspaceManager(aura::Window* viewport);
  virtual ~WorkspaceManager();

  // Returns true if |window| should be managed by the WorkspaceManager. Use
  // Contains() to test if the Window is currently managed by WorkspaceManager.
  static bool ShouldManageWindow(aura::Window* window);

  // Returns true if |window| has been added to this WorkspaceManager.
  bool Contains(aura::Window* window) const;

  // Adds/removes a window creating/destroying workspace as necessary.
  void AddWindow(aura::Window* window);
  void RemoveWindow(aura::Window* window);

  // Returns the Window this WorkspaceManager controls.
  aura::Window* contents_view() { return contents_view_; }

  // Updates the visibility and whether any windows overlap the shelf.
  void UpdateShelfVisibility();

  // Invoked when the show state of the specified window changes.
  void ShowStateChanged(aura::Window* window);

  // BaseWorkspaceManager overrides:
  virtual bool IsInMaximizedMode() const OVERRIDE;
  virtual WorkspaceWindowState GetWindowState() const OVERRIDE;
  virtual void SetShelf(ShelfLayoutManager* shelf) OVERRIDE;
  virtual void SetActiveWorkspaceByWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetParentForNewWindow(aura::Window* window) OVERRIDE;

 private:
  // Enumeration of whether windows should animate or not.
  enum AnimateChangeType {
    ANIMATE,
    DONT_ANIMATE
  };

  friend class Workspace;
  friend class WorkspaceManagerTest;

  void AddWorkspace(Workspace* workspace);
  void RemoveWorkspace(Workspace* workspace);

  // Sets the visibility of the windows in |workspace|.
  void SetVisibilityOfWorkspaceWindows(Workspace* workspace,
                                       AnimateChangeType change_type,
                                       bool value);

  // Implementation of SetVisibilityOfWorkspaceWindows().
  void SetWindowLayerVisibility(const std::vector<aura::Window*>& windows,
                                AnimateChangeType change_type,
                                bool value);

  // Returns the active workspace.
  Workspace* GetActiveWorkspace() const;

  // Returns the workspace that contanis the |window|.
  Workspace* FindBy(aura::Window* window) const;

  // Sets the active workspace.
  void SetActiveWorkspace(Workspace* workspace);

  // Returns the bounds of the work area.
  gfx::Rect GetWorkAreaBounds() const;

  // Returns the index of the workspace that contains the |window|.
  int GetWorkspaceIndexContaining(aura::Window* window) const;

  // Sets the bounds of |window|. This sets |ignored_window_| to |window| so
  // that the bounds change is allowed through.
  void SetWindowBounds(aura::Window* window, const gfx::Rect& bounds);

  // Invoked when the type of workspace needed for |window| changes.
  void OnTypeOfWorkspacedNeededChanged(aura::Window* window);

  // Returns the Workspace whose type is TYPE_MANAGED, or NULL if there isn't
  // one.
  Workspace* GetManagedWorkspace();

  // Creates a new workspace of the specified type.
  Workspace* CreateWorkspace(Workspace::Type type);

  // Deletes workspaces of TYPE_MAXIMIZED when they become empty.
  void CleanupWorkspace(Workspace* workspace);

  aura::Window* contents_view_;

  Workspace* active_workspace_;

  std::vector<Workspace*> workspaces_;

  // Window being maximized or restored during a workspace type change.
  // It has its own animation and is ignored by workspace show/hide animations.
  aura::Window* maximize_restore_window_;

  // See description above setter.
  int grid_size_;

  // Owned by the Shell container window LauncherContainer. May be NULL.
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_MANAGER_H_
