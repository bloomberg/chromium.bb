// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_H_
#define ASH_WM_WORKSPACE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "ash/ash_export.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class WorkspaceManager;
class WorkspaceTest;

// A workspace contains a number of windows. The number of windows a Workspace
// may contain is dictated by the type. Typically only one workspace is visible
// at a time. The exception to that is when overview mode is active.
class ASH_EXPORT Workspace {
 public:
  // Type of workspace. The type of workspace dictates the types of windows the
  // workspace can contain.
  enum Type {
    // The workspace holds a single maximized or full screen window.
    TYPE_MAXIMIZED,

    // Workspace contains multiple windows that are split (also known as
    // co-maximized).
    TYPE_SPLIT,

    // Workspace contains non-maximized windows that can be moved in anyway.
    TYPE_NORMAL,
  };

  // Specifies the direction to shift windows in |ShiftWindows()|.
  enum ShiftDirection {
    SHIFT_TO_RIGHT,
    SHIFT_TO_LEFT
  };

  explicit Workspace(WorkspaceManager* manager);
  virtual ~Workspace();

  // Returns the type of workspace that can contain |window|.
  static Type TypeForWindow(aura::Window* window);

  // The type of this Workspace.
  void SetType(Type type);
  Type type() const { return type_; }

  // Returns true if this workspace has no windows.
  bool is_empty() const { return windows_.empty(); }
  size_t num_windows() const { return windows_.size(); }
  const std::vector<aura::Window*>& windows() const { return windows_; }

  // Invoked when the size of the workspace changes.
  void WorkspaceSizeChanged();

  // Returns the work area bounds of this workspace in viewport coordinates.
  gfx::Rect GetWorkAreaBounds() const;

  // Adds the |window| at the position after the window |after|.  It
  // inserts at the end if |after| is NULL. Return true if the
  // |window| was successfully added to this workspace, or false if it
  // failed.
  bool AddWindowAfter(aura::Window* window, aura::Window* after);

  // Removes |window| from this workspace.
  void RemoveWindow(aura::Window* window);

  // Return true if this workspace has |window|.
  bool Contains(aura::Window* window) const;

  // Activates this workspace.
  void Activate();

  // Returns true if the workspace contains a fullscreen window.
  bool ContainsFullscreenWindow() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, WorkspaceBasic);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, RotateWindows);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, ShiftWindowsSingle);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceTest, ShiftWindowsMultiple);
  FRIEND_TEST_ALL_PREFIXES(WorkspaceManagerTest, RotateWindows);

  // Returns the index in layout order of |window| in this workspace.
  int GetIndexOf(aura::Window* window) const;

  // Returns true if the given |window| can be added to this workspace.
  bool CanAdd(aura::Window* window) const;

  Type type_;

  WorkspaceManager* workspace_manager_;

  // Windows in the workspace in layout order.
  std::vector<aura::Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(Workspace);
};

typedef std::vector<Workspace*> Workspaces;

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_H_
