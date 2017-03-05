// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_TEST_HELPER_H_
#define ASH_WM_WORKSPACE_CONTROLLER_TEST_HELPER_H_

#include "ash/common/wm/workspace_controller.h"
#include "base/macros.h"

namespace ash {
class MultiWindowResizeController;
class WorkspaceEventHandler;

class WorkspaceControllerTestHelper {
 public:
  explicit WorkspaceControllerTestHelper(WorkspaceController* controller);
  ~WorkspaceControllerTestHelper();

  WorkspaceEventHandler* GetEventHandler();
  MultiWindowResizeController* GetMultiWindowResizeController();

 private:
  WorkspaceController* controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceControllerTestHelper);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_TEST_HELPER_H_
