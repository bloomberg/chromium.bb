// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_ROOT_WINDOW_CONTROLLER_COMMON_H_
#define ASH_COMMON_ROOT_WINDOW_CONTROLLER_COMMON_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

class WmWindow;
class WorkspaceController;

namespace wm {
class RootWindowLayoutManager;
}

// This will eventually become what is RootWindowController. During the
// transition it contains code used by both the aura and mus implementations.
// It should *not* contain any aura specific code.
class ASH_EXPORT RootWindowControllerCommon {
 public:
  explicit RootWindowControllerCommon(WmWindow* root);
  ~RootWindowControllerCommon();

  // Creates the containers (WmWindows) used by the shell.
  void CreateContainers();

  // Creates the LayoutManagers for the windows created by CreateContainers().
  void CreateLayoutManagers();

  void DeleteWorkspaceController();

  wm::RootWindowLayoutManager* root_window_layout() {
    return root_window_layout_;
  }

  WorkspaceController* workspace_controller() {
    return workspace_controller_.get();
  }

 private:
  WmWindow* root_;

  wm::RootWindowLayoutManager* root_window_layout_;

  std::unique_ptr<WorkspaceController> workspace_controller_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowControllerCommon);
};

}  // namespace ash

#endif  // ASH_COMMON_ROOT_WINDOW_CONTROLLER_COMMON_H_
