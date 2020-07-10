// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_TEST_HELPER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_TEST_HELPER_H_

#include "ash/wm/workspace/workspace_event_handler.h"

#include "base/macros.h"

namespace ash {

class WorkspaceEventHandlerTestHelper {
 public:
  explicit WorkspaceEventHandlerTestHelper(WorkspaceEventHandler* handler);
  ~WorkspaceEventHandlerTestHelper();

  MultiWindowResizeController* resize_controller() {
    return &(handler_->multi_window_resize_controller_);
  }

 private:
  WorkspaceEventHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandlerTestHelper);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_HANDLER_TEST_HELPER_H_
