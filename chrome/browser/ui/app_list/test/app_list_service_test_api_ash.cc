// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/app_list_service_test_api_ash.h"

#include "ash/shell.h"
#include "ash/test/app_list_controller_test_api.h"
#include "ui/app_list/views/app_list_view.h"

namespace test {

app_list::AppListModel* AppListServiceTestApiAsh::GetAppListModel() {
  return ash::test::AppListControllerTestApi(ash::Shell::GetInstance())
      .view()->model();
}

#if !defined(OS_WIN)
// static
scoped_ptr<AppListServiceTestApi> AppListServiceTestApi::Create(
    chrome::HostDesktopType desktop) {
  DCHECK_EQ(chrome::HOST_DESKTOP_TYPE_ASH, desktop);
  return scoped_ptr<AppListServiceTestApi>(
      new AppListServiceTestApiAsh()).Pass();
}
#endif  // !defined(OS_WIN)

}  // namespace test
