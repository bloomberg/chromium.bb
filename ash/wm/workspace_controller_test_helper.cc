// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller_test_helper.h"

#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace/workspace_event_handler_test_helper.h"
#include "ui/aura/window.h"

namespace ash {

WorkspaceControllerTestHelper::WorkspaceControllerTestHelper(
    WorkspaceController* controller)
    : controller_(controller) {
}

WorkspaceControllerTestHelper::~WorkspaceControllerTestHelper() {
}

WorkspaceEventHandler* WorkspaceControllerTestHelper::GetEventHandler() {
  return controller_->event_handler_.get();
}

MultiWindowResizeController*
WorkspaceControllerTestHelper::GetMultiWindowResizeController() {
  return WorkspaceEventHandlerTestHelper(GetEventHandler()).resize_controller();
}

}  // namespace ash
