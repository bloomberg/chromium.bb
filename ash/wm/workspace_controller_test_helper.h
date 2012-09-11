// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_TEST_HELPER_H_
#define ASH_WM_WORKSPACE_CONTROLLER_TEST_HELPER_H_

#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace/workspace_manager2.h"

namespace ash {
namespace internal {

class MultiWindowResizeController;
class WorkspaceEventHandler;

class WorkspaceControllerTestHelper {
 public:
  explicit WorkspaceControllerTestHelper(WorkspaceController* controller);
  ~WorkspaceControllerTestHelper();

  WorkspaceEventHandler* GetEventHandler();
  MultiWindowResizeController* GetMultiWindowResizeController();
  WorkspaceManager* workspace_manager() {
    return static_cast<WorkspaceManager*>(
        controller_->workspace_manager_.get());
  }
  WorkspaceManager2* workspace_manager2() {
    return static_cast<WorkspaceManager2*>(
        controller_->workspace_manager_.get());
  }

 private:
  WorkspaceController* controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceControllerTestHelper);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_TEST_HELPER_H_
