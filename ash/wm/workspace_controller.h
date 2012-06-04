// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/activation_change_observer.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class WorkspaceControllerTestHelper;
class WorkspaceEventFilter;
class WorkspaceLayoutManager;
class WorkspaceManager;

// WorkspaceController acts as a central place that ties together all the
// various workspace pieces: WorkspaceManager, WorkspaceLayoutManager and
// WorkspaceEventFilter.
class ASH_EXPORT WorkspaceController :
    public aura::client::ActivationChangeObserver {
 public:
  explicit WorkspaceController(aura::Window* viewport);
  virtual ~WorkspaceController();

  // Returns the workspace manager that this controller owns.
  WorkspaceManager* workspace_manager() {
    return workspace_manager_.get();
  }

  // Sets the size of the grid.
  void SetGridSize(int grid_size);

  // aura::client::ActivationChangeObserver overrides:
  virtual void OnWindowActivated(aura::Window* window,
                                 aura::Window* old_active) OVERRIDE;

 private:
  friend class WorkspaceControllerTestHelper;

  aura::Window* viewport_;

  scoped_ptr<WorkspaceManager> workspace_manager_;

  // Owned by the window its attached to.
  WorkspaceLayoutManager* layout_manager_;

  // Owned by |viewport_|.
  WorkspaceEventFilter* event_filter_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_H_
