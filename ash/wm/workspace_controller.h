// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
}

namespace ash {
class ShelfLayoutManager;
class WorkspaceControllerTestHelper;
class WorkspaceEventHandler;
class WorkspaceLayoutManager;
class WorkspaceLayoutManagerDelegate;

// WorkspaceController acts as a central place that ties together all the
// various workspace pieces.
class ASH_EXPORT WorkspaceController {
 public:
  explicit WorkspaceController(aura::Window* viewport);
  virtual ~WorkspaceController();

  // Returns the current window state.
  WorkspaceWindowState GetWindowState() const;

  void SetShelf(ShelfLayoutManager* shelf);

  // Starts the animation that occurs on first login.
  void DoInitialAnimation();

  // Add a delegate which adds a backdrop behind the top window of the default
  // workspace.
  void SetMaximizeBackdropDelegate(
      scoped_ptr<WorkspaceLayoutManagerDelegate> delegate);

  WorkspaceLayoutManager* layout_manager() { return layout_manager_; }

 private:
  friend class WorkspaceControllerTestHelper;

  aura::Window* viewport_;

  ShelfLayoutManager* shelf_;
  scoped_ptr<WorkspaceEventHandler> event_handler_;
  WorkspaceLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceController);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_H_
