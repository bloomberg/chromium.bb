// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_presenter_service.h"

#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "ui/app_list/presenter/app_list_presenter.h"

AppListPresenterService::AppListPresenterService() {}
AppListPresenterService::~AppListPresenterService() {}

void AppListPresenterService::Show(int64_t display_id) {
  GetPresenter()->Show(display_id);
}

void AppListPresenterService::Dismiss() {
  GetPresenter()->Dismiss();
}

void AppListPresenterService::ToggleAppList(int64_t display_id) {
  GetPresenter()->ToggleAppList(display_id);
}

app_list::AppListPresenter* AppListPresenterService::GetPresenter() {
  return AppListServiceAsh::GetInstance()->GetAppListPresenter();
}
