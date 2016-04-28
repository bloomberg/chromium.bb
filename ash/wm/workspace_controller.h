// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/common/workspace/workspace_types.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {
class ShelfLayoutManager;
class WorkspaceControllerTestHelper;
class WorkspaceEventHandler;
class WorkspaceLayoutManager;
class WorkspaceLayoutManagerBackdropDelegate;

namespace wm {
class WorkspaceLayoutManagerDelegate;
}

// WorkspaceController acts as a central place that ties together all the
// various workspace pieces.
class ASH_EXPORT WorkspaceController {
 public:
  WorkspaceController(
      aura::Window* viewport,
      std::unique_ptr<wm::WorkspaceLayoutManagerDelegate> delegate);
  virtual ~WorkspaceController();

  // Returns the current window state.
  wm::WorkspaceWindowState GetWindowState() const;

  void SetShelf(ShelfLayoutManager* shelf);

  // Starts the animation that occurs on first login.
  void DoInitialAnimation();

  // Add a delegate which adds a backdrop behind the top window of the default
  // workspace.
  void SetMaximizeBackdropDelegate(
      std::unique_ptr<WorkspaceLayoutManagerBackdropDelegate> delegate);

  WorkspaceLayoutManager* layout_manager() { return layout_manager_; }

 private:
  friend class WorkspaceControllerTestHelper;

  aura::Window* viewport_;

  ShelfLayoutManager* shelf_;
  std::unique_ptr<WorkspaceEventHandler> event_handler_;

  // Owned by |viewport_|.
  WorkspaceLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceController);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_H_
