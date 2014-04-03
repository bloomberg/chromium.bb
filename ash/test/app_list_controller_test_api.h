// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_APP_LIST_CONTROLLER_TEST_API_H_
#define ASH_TEST_APP_LIST_CONTROLLER_TEST_API_H_

#include "base/basictypes.h"

namespace app_list {
class AppListView;
class AppsGridView;
}

namespace ash {
class AppListController;
class Shell;

namespace test {

// Accesses private data from an AppListController for testing.
class AppListControllerTestApi {
 public:
  explicit AppListControllerTestApi(Shell* shell);

  // Gets the root level apps grid view.
  app_list::AppsGridView* GetRootGridView();

  app_list::AppListView* view();

 private:
  AppListController* app_list_controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(AppListControllerTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_APP_LIST_CONTROLLER_TEST_API_H_
