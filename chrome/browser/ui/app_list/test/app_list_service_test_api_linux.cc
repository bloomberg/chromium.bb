// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service_linux.h"
#include "chrome/browser/ui/app_list/test/app_list_service_test_api.h"

namespace test {
namespace {

class AppListServiceTestApiLinux : public AppListServiceTestApi {
 public:
  AppListServiceTestApiLinux();

  virtual app_list::AppListModel* GetAppListModel() OVERRIDE;

 private:
  AppListServiceLinux* app_list_service_linux_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceTestApiLinux);
};

AppListServiceTestApiLinux::AppListServiceTestApiLinux()
    : app_list_service_linux_(AppListServiceLinux::GetInstance()) {}

app_list::AppListModel* AppListServiceTestApiLinux::GetAppListModel() {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace

// static
scoped_ptr<AppListServiceTestApi> AppListServiceTestApi::Create(
    chrome::HostDesktopType desktop) {

  DCHECK_EQ(chrome::HOST_DESKTOP_TYPE_NATIVE, desktop);
  return scoped_ptr<AppListServiceTestApi>(
      new AppListServiceTestApiLinux()).Pass();
}

}  // namespace test
