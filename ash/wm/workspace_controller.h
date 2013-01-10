// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/activation_change_observer.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class ShelfLayoutManager;
class WorkspaceControllerTestHelper;
class WorkspaceCycler;
class WorkspaceEventHandler;
class WorkspaceManager;

// WorkspaceController acts as a central place that ties together all the
// various workspace pieces.
class ASH_EXPORT WorkspaceController
    : public aura::client::ActivationChangeObserver {
 public:
  explicit WorkspaceController(aura::Window* viewport);
  virtual ~WorkspaceController();

  // Returns the current window state.
  WorkspaceWindowState GetWindowState() const;

  void SetShelf(ShelfLayoutManager* shelf);

  // Sets the active workspace based on |window|.
  void SetActiveWorkspaceByWindow(aura::Window* window);

  // Returns the container window for the active workspace, never NULL.
  aura::Window* GetActiveWorkspaceWindow();

  // See description in WorkspaceManager::GetParentForNewWindow().
  aura::Window* GetParentForNewWindow(aura::Window* window);

  // Starts the animation that occurs on first login.
  void DoInitialAnimation();

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

 private:
  friend class WorkspaceControllerTestHelper;

  aura::Window* viewport_;

  scoped_ptr<WorkspaceManager> workspace_manager_;

  // Cycles through the WorkspaceManager's workspaces in response to a three
  // finger vertical scroll.
  scoped_ptr<WorkspaceCycler> workspace_cycler_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_H_
