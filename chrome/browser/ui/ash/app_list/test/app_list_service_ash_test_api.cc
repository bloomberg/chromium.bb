// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/test/app_list_service_ash_test_api.h"

#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "ui/app_list/presenter/test/app_list_presenter_impl_test_api.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/start_page_view.h"

AppListServiceAshTestApi::AppListServiceAshTestApi() {}

app_list::AppListPresenterImpl* AppListServiceAshTestApi::GetAppListPresenter()
    const {
  return AppListServiceAsh::GetInstance()->app_list_presenter_.get();
}

app_list::AppListView* AppListServiceAshTestApi::GetAppListView() const {
  app_list::test::AppListPresenterImplTestApi presenter_test_api(
      GetAppListPresenter());
  return presenter_test_api.view();
}

app_list::AppsGridView* AppListServiceAshTestApi::GetRootGridView() const {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->apps_container_view()
      ->apps_grid_view();
}

app_list::StartPageView* AppListServiceAshTestApi::GetStartPageView() const {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->start_page_view();
}

void AppListServiceAshTestApi::LayoutContentsView() {
  GetAppListView()->app_list_main_view()->contents_view()->Layout();
}
