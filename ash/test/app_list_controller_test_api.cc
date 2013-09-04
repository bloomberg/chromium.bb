// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/app_list_controller_test_api.h"

#include "ash/test/shell_test_api.h"
#include "ash/wm/app_list_controller.h"

namespace ash {
namespace test {

AppListControllerTestApi::AppListControllerTestApi(Shell* shell)
    : app_list_controller_(ShellTestApi(shell).app_list_controller()) {}

app_list::AppListView* AppListControllerTestApi::view() {
  return app_list_controller_->view_;
}

}  // namespace test
}  // namespace ash
