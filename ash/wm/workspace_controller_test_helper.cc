// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller_test_helper.h"

#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace/workspace_event_handler_test_helper.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

WorkspaceControllerTestHelper::WorkspaceControllerTestHelper(
    WorkspaceController* controller)
    : controller_(controller) {
}

WorkspaceControllerTestHelper::~WorkspaceControllerTestHelper() {
}

WorkspaceEventHandler* WorkspaceControllerTestHelper::GetEventHandler() {
  if (WorkspaceController::IsWorkspace2Enabled()) {
    ui::EventTarget::TestApi test_api(controller_->viewport_->children()[0]);
    return static_cast<WorkspaceEventHandler*>(
        test_api.pre_target_handlers().front());
  }
  return controller_->event_handler_;
}

MultiWindowResizeController*
WorkspaceControllerTestHelper::GetMultiWindowResizeController() {
  return WorkspaceEventHandlerTestHelper(GetEventHandler()).resize_controller();
}

}  // namespace internal
}  // namespace ash
