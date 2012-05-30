// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_H_
#define ASH_WM_WORKSPACE_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class WorkspaceManager;

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

    // Workspace contains non-maximized windows.
    TYPE_MANAGED,
  };

  Workspace(WorkspaceManager* manager, Type type);
  virtual ~Workspace();

  // Returns the type of workspace that can contain |window|.
  static Type TypeForWindow(aura::Window* window);

  Type type() const { return type_; }

  // Returns true if this workspace has no windows.
  bool is_empty() const { return windows_.empty(); }
  size_t num_windows() const { return windows_.size(); }
  const std::vector<aura::Window*>& windows() const { return windows_; }

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

 protected:
  // Sets the bounds of the specified window.
  void SetWindowBounds(aura::Window* window, const gfx::Rect& bounds);

  // Returns true if the given |window| can be added to this workspace.
  virtual bool CanAdd(aura::Window* window) const = 0;

  // Invoked from AddWindowAfter().
  virtual void OnWindowAddedAfter(aura::Window* window,
                                  aura::Window* after) = 0;

  // Invoked from RemoveWindow().
  virtual void OnWindowRemoved(aura::Window* window) = 0;

 private:
  const Type type_;

  WorkspaceManager* workspace_manager_;

  // Windows in the workspace in layout order.
  std::vector<aura::Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(Workspace);
};

typedef std::vector<Workspace*> Workspaces;

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_H_
