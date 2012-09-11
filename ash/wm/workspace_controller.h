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

class BaseWorkspaceManager;
class ShelfLayoutManager;
class WorkspaceControllerTestHelper;
class WorkspaceEventHandler;
class WorkspaceLayoutManager;

// WorkspaceController acts as a central place that ties together all the
// various workspace pieces.
class ASH_EXPORT WorkspaceController
    : public aura::client::ActivationChangeObserver {
 public:
  explicit WorkspaceController(aura::Window* viewport);
  virtual ~WorkspaceController();

  // Returns true if Workspace2 is enabled.
  static bool IsWorkspace2Enabled();

  // Returns true if in maximized or fullscreen mode.
  bool IsInMaximizedMode() const;

  // Returns the current window state.
  WorkspaceWindowState GetWindowState() const;

  void SetShelf(ShelfLayoutManager* shelf);

  // Sets the active workspace based on |window|.
  void SetActiveWorkspaceByWindow(aura::Window* window);

  // See description in BaseWorkspaceManager::GetParentForNewWindow().
  aura::Window* GetParentForNewWindow(aura::Window* window);

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* window,
                                 aura::Window* old_active) OVERRIDE;

 private:
  friend class WorkspaceControllerTestHelper;

  aura::Window* viewport_;

  scoped_ptr<BaseWorkspaceManager> workspace_manager_;

  // TODO(sky): remove |layout_manager_| and |event_handler_| when Workspace2
  // is the default.

  // Owned by the window its attached to.
  WorkspaceLayoutManager* layout_manager_;

  // Owned by |viewport_|.
  WorkspaceEventHandler* event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_H_
