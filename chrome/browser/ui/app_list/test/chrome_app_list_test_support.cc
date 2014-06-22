// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"

#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"

namespace test {

app_list::AppListModel* GetAppListModel(AppListService* service) {
  return app_list::AppListSyncableServiceFactory::GetForProfile(
      service->GetCurrentAppListProfile())->model();
}

AppListService* GetAppListService() {
  // TODO(tapted): Consider testing ash explicitly on the win-ash trybot.
  return AppListService::Get(chrome::GetActiveDesktop());
}

}  // namespace test
