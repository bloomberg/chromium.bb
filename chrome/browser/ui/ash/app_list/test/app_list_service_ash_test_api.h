// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_TEST_APP_LIST_SERVICE_ASH_TEST_API_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_TEST_APP_LIST_SERVICE_ASH_TEST_API_H_

#include "base/macros.h"

namespace app_list {
class AppListPresenterImpl;
class AppListView;
class AppsGridView;
class StartPageView;
}  // namespace app_list

// Accesses private data from an AppListServiceAsh and AppListPresenterImpl
// for testing.
class AppListServiceAshTestApi {
 public:
  AppListServiceAshTestApi();

  app_list::AppListPresenterImpl* GetAppListPresenter() const;

  app_list::AppListView* GetAppListView() const;

  // Gets the root level apps grid view.
  app_list::AppsGridView* GetRootGridView() const;

  // Gets the start page view.
  app_list::StartPageView* GetStartPageView() const;

  // Calls Layout() on the app_list::ContentsView.
  void LayoutContentsView();

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListServiceAshTestApi);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_TEST_APP_LIST_SERVICE_ASH_TEST_API_H_
