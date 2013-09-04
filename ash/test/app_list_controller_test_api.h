// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_APP_LIST_CONTROLLER_TEST_API_H_
#define ASH_TEST_APP_LIST_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace app_list {
class AppListView;
}

namespace ash {
class Shell;
namespace internal {
class AppListController;
}

namespace test {

// Accesses private data from an AppListController for testing.
class ASH_EXPORT AppListControllerTestApi {
 public:
  explicit AppListControllerTestApi(Shell* shell);

  app_list::AppListView* view();

 private:
  internal::AppListController* app_list_controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(AppListControllerTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_APP_LIST_CONTROLLER_TEST_API_H_
