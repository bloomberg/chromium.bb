// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/app_list/app_list_service_mac.h"
#include "chrome/browser/ui/app_list/test/app_list_service_test_api.h"
#import "ui/app_list/cocoa/app_list_view_controller.h"
#import "ui/app_list/cocoa/app_list_window_controller.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"

namespace test {

class AppListServiceTestApiMac : public AppListServiceTestApi {
 public:
  AppListServiceTestApiMac();

  virtual app_list::AppListModel* GetAppListModel() OVERRIDE;

 private:
  AppListServiceMac* app_list_service_mac_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceTestApiMac);
};

AppListServiceTestApiMac::AppListServiceTestApiMac()
    : app_list_service_mac_(AppListServiceMac::GetInstance()) {}

app_list::AppListModel* AppListServiceTestApiMac::GetAppListModel() {
  AppListViewController* view_controller =
      [app_list_service_mac_->window_controller_ appListViewController];
  return [[view_controller appsGridController] model];
}

// static
scoped_ptr<AppListServiceTestApi> AppListServiceTestApi::Create(
    chrome::HostDesktopType desktop) {
  DCHECK_EQ(chrome::HOST_DESKTOP_TYPE_NATIVE, desktop);
  return scoped_ptr<AppListServiceTestApi>(
      new AppListServiceTestApiMac()).Pass();
}

}  // namespace test
