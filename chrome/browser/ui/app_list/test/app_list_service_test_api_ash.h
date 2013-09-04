// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SERVICE_TEST_API_ASH_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SERVICE_TEST_API_ASH_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/app_list/test/app_list_service_test_api.h"

namespace test {

class AppListServiceTestApiAsh : public AppListServiceTestApi {
 public:
  AppListServiceTestApiAsh() {}

  virtual app_list::AppListModel* GetAppListModel() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceTestApiAsh);
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SERVICE_TEST_API_ASH_H_
