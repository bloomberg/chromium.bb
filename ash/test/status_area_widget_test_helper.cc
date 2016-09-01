// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/status_area_widget_test_helper.h"

#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"

namespace ash {

LoginStatus StatusAreaWidgetTestHelper::GetUserLoginStatus() {
  return WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
}

StatusAreaWidget* StatusAreaWidgetTestHelper::GetStatusAreaWidget() {
  return Shell::GetPrimaryRootWindowController()->GetStatusAreaWidget();
}

StatusAreaWidget* StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget() {
  RootWindowController* primary_controller =
      Shell::GetPrimaryRootWindowController();
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (size_t i = 0; i < controllers.size(); ++i) {
    if (controllers[i] != primary_controller)
      return controllers[i]->GetStatusAreaWidget();
  }

  return nullptr;
}

}  // namespace ash
