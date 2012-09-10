// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_BASE_WORKSPACE_MANAGER_H_
#define ASH_WM_WORKSPACE_BASE_WORKSPACE_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace_types.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class ShelfLayoutManager;

// TODO: this is temporary until WorkspaceManager2 is the default.
class ASH_EXPORT BaseWorkspaceManager {
 public:
  virtual ~BaseWorkspaceManager() {}

  // Returns true if in maximized or fullscreen mode.
  virtual bool IsInMaximizedMode() const = 0;

  // Returns the current window state.
  virtual WorkspaceWindowState GetWindowState() const = 0;

  virtual void SetShelf(ShelfLayoutManager* shelf) = 0;

  // Activates the workspace containing |window|. Does nothing if |window| is
  // NULL or not contained in a workspace.
  virtual void SetActiveWorkspaceByWindow(aura::Window* window) = 0;

  // Returns the parent for |window|. This is invoked from StackingController
  // when a new Window is being added.
  virtual aura::Window* GetParentForNewWindow(aura::Window* window) = 0;
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_BASE_WORKSPACE_MANAGER_H_
