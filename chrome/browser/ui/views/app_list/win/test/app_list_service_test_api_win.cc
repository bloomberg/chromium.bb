// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/app_list_service_test_api.h"
#include "chrome/browser/ui/app_list/test/app_list_service_test_api_ash.h"
#include "chrome/browser/ui/views/app_list/win/app_list_service_win.h"

namespace test {
namespace {

class AppListServiceTestApiWin : public AppListServiceTestApi {
 public:
  AppListServiceTestApiWin() {}

  virtual app_list::AppListModel* GetAppListModel() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceTestApiWin);
};

app_list::AppListModel* AppListServiceTestApiWin::GetAppListModel() {
  AppListServiceWin* app_list_service_win =
      static_cast<AppListServiceWin*>(chrome::GetAppListServiceWin());
  return app_list_service_win->GetAppListModelForTesting();
}

}  // namespace

// static
scoped_ptr<AppListServiceTestApi> AppListServiceTestApi::Create(
    chrome::HostDesktopType desktop) {
#if defined(USE_ASH)
  if (desktop == chrome::HOST_DESKTOP_TYPE_ASH) {
    return scoped_ptr<AppListServiceTestApi>(
        new AppListServiceTestApiAsh()).Pass();
  }
#endif  // defined(USE_ASH)

  DCHECK_EQ(chrome::HOST_DESKTOP_TYPE_NATIVE, desktop);
  return scoped_ptr<AppListServiceTestApi>(
      new AppListServiceTestApiWin()).Pass();
}

}  // namespace test
