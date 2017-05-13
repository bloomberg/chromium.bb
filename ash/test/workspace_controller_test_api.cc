// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/workspace_controller_test_api.h"

#include "ash/test/workspace_event_handler_test_helper.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

WorkspaceControllerTestApi::WorkspaceControllerTestApi(
    WorkspaceController* controller)
    : controller_(controller) {}

WorkspaceControllerTestApi::~WorkspaceControllerTestApi() {}

WorkspaceEventHandler* WorkspaceControllerTestApi::GetEventHandler() {
  return controller_->event_handler_.get();
}

MultiWindowResizeController*
WorkspaceControllerTestApi::GetMultiWindowResizeController() {
  return WorkspaceEventHandlerTestHelper(GetEventHandler()).resize_controller();
}

}  // namespace test
}  // namespace ash
