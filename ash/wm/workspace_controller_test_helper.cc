// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller_test_helper.h"

#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace/workspace_event_filter_test_helper.h"

namespace ash {
namespace internal {

WorkspaceControllerTestHelper::WorkspaceControllerTestHelper(
    WorkspaceController* controller)
    : controller_(controller) {
}

WorkspaceControllerTestHelper::~WorkspaceControllerTestHelper() {
}

MultiWindowResizeController*
WorkspaceControllerTestHelper::GetMultiWindowResizeController() {
  return WorkspaceEventFilterTestHelper(filter()).resize_controller();
}

}  // namespace internal
}  // namespace ash
