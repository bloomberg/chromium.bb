// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SERVICE_TEST_API_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SERVICE_TEST_API_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/host_desktop.h"

namespace app_list {
class AppListModel;
}

namespace test {

// Accesses private data from an AppListService for testing.
class AppListServiceTestApi {
 public:
  AppListServiceTestApi() {}
  virtual ~AppListServiceTestApi() {}

  static scoped_ptr<AppListServiceTestApi> Create(
      chrome::HostDesktopType desktop);

  virtual app_list::AppListModel* GetAppListModel() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceTestApi);
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_APP_LIST_SERVICE_TEST_API_H_
