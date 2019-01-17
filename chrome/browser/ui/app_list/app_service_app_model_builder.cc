// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service_app_model_builder.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ui/app_list/app_service_app_item.h"

AppServiceAppModelBuilder::AppServiceAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, AppServiceAppItem::kItemType) {}

AppServiceAppModelBuilder::~AppServiceAppModelBuilder() = default;

void AppServiceAppModelBuilder::BuildModel() {
  apps::AppServiceProxy* proxy = apps::AppServiceProxy::Get(profile());
  if (proxy) {
    proxy->Cache().ForEachApp(
        [this](const apps::AppUpdate& update) { OnAppUpdate(update); });
    Observe(&proxy->Cache());
  } else {
    // TODO(crbug.com/826982): do we want apps in incognito mode? See the TODO
    // in AppServiceProxyFactory::GetForProfile about whether
    // apps::AppServiceProxy::Get should return nullptr for incognito profiles.
  }
}

void AppServiceAppModelBuilder::OnAppUpdate(const apps::AppUpdate& update) {
  ChromeAppListItem* item = GetAppItem(update.AppId());
  bool show = (update.Readiness() == apps::mojom::Readiness::kReady) &&
              (update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue);

  if (item) {
    if (show) {
      DCHECK(item->GetItemType() == AppServiceAppItem::kItemType);
      static_cast<AppServiceAppItem*>(item)->OnAppUpdate(update);

      // TODO(crbug.com/826982): drop the check for kExtension or kWeb, and
      // call UpdateItem unconditionally?
      apps::mojom::AppType app_type = update.AppType();
      if ((app_type == apps::mojom::AppType::kExtension) ||
          (app_type == apps::mojom::AppType::kWeb)) {
        app_list::AppListSyncableService* serv = service();
        if (serv) {
          serv->UpdateItem(item);
        }
      }

    } else {
      RemoveApp(update.AppId(), false /* unsynced_change */);
    }

  } else if (show) {
    InsertApp(std::make_unique<AppServiceAppItem>(
        profile(), model_updater(), GetSyncItem(update.AppId()), update));
  }
}
