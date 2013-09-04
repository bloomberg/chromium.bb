// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_WIN_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_WIN_H_

#include "chrome/browser/ui/app_list/app_list_service_impl.h"

namespace app_list{
class AppListModel;
}

// Exposes methods required by the AppListServiceTestApi on Windows.
// TODO(tapted): Put the full declaration for Windows here, and remove testing
// methods once they can access the implementation from the test api.
class AppListServiceWin : public AppListServiceImpl {
 public:
  AppListServiceWin() {}

  virtual app_list::AppListModel* GetAppListModelForTesting() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceWin);
};

namespace chrome {

AppListService* GetAppListServiceWin();

// Returns the resource id of the app list PNG icon used for the taskbar,
// infobars and window decorations.
int GetAppListIconResourceId();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_WIN_H_
