// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MANAGER_H_
#define ASH_WM_WORKSPACE_MANAGER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace.h"
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
class ASH_EXPORT WorkspaceManager {
 public:
  enum WindowState {
    // There's a full screen window.
    WINDOW_STATE_FULL_SCREEN,

    // There's a maximized window.
    WINDOW_STATE_MAXIMIZED,

    // At least one window overlaps the shelf.
    WINDOW_STATE_WINDOW_OVERLAPS_SHELF,

    // None of the windows are fullscreen, maximized or touch the shelf.
    WINDOW_STATE_DEFAULT,
  };

  explicit WorkspaceManager(aura::Window* viewport);
  virtual ~WorkspaceManager();

  // Returns true if |window| should be managed by the WorkspaceManager.
  bool IsManagedWindow(aura::Window* window) const;

  // Returns true if the |window| is managed by the WorkspaceManager.
  bool IsManagingWindow(aura::Window* window) const;

  // Returns true if in maximized or fullscreen mode.
  bool IsInMaximizedMode() const;

  // Adds/removes a window creating/destroying workspace as necessary.
  void AddWindow(aura::Window* window);
  void RemoveWindow(aura::Window* window);

  // Activates the workspace containing |window|. Does nothing if |window| is
  // NULL or not contained in a workspace.
  void SetActiveWorkspaceByWindow(aura::Window* window);

  // Returns the Window this WorkspaceManager controls.
  aura::Window* contents_view() { return contents_view_; }

  // Returns the window for rotate operation based on the |location|.
  // TODO: this isn't currently used; remove if we do away with overview.
  aura::Window* FindRotateWindowForLocation(const gfx::Point& location);

  // Returns the bounds in which a window can be moved/resized.
  gfx::Rect GetDragAreaBounds();

  // Sets the size of the grid. Newly added windows are forced to align to the
  // size of the grid.
  void set_grid_size(int size) { grid_size_ = size; }
  int grid_size() const { return grid_size_; }

  void set_shelf(ShelfLayoutManager* shelf) { shelf_ = shelf; }

  // Updates the visibility and whether any windows overlap the shelf.
  void UpdateShelfVisibility();

  // Returns the current window state.
  WindowState GetWindowState();

  // Invoked when the show state of the specified window changes.
  void ShowStateChanged(aura::Window* window);

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

#endif  // ASH_WM_WORKSPACE_MANAGER_H_
