// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service_app_item.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"

// static
const char AppServiceAppItem::kItemType[] = "AppServiceAppItem";

AppServiceAppItem::AppServiceAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const apps::AppUpdate& app_update)
    : ChromeAppListItem(profile, app_update.AppId()) {
  SetName(app_update.Name());
  apps::AppServiceProxy* proxy = apps::AppServiceProxy::Get(profile);
  if (proxy) {
    // TODO(crbug.com/826982): if another AppUpdate is observed, we should call
    // LoadIcon again. The question (see the TODO in
    // AppServiceAppModelBuilder::OnAppUpdate) is who should be the observer:
    // the AppModelBuilder or the AppItem?
    proxy->LoadIcon(app_update.AppId(),
                    apps::mojom::IconCompression::kUncompressed,
                    app_list::AppListConfig::instance().grid_icon_dimension(),
                    base::BindOnce(&AppServiceAppItem::OnLoadIcon,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (sync_item && sync_item->item_ordinal.IsValid()) {
    UpdateFromSync(sync_item);
  } else {
    SetDefaultPositionIfApplicable(model_updater);
  }

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

AppServiceAppItem::~AppServiceAppItem() = default;

const char* AppServiceAppItem::GetItemType() const {
  return AppServiceAppItem::kItemType;
}

void AppServiceAppItem::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }
  SetIcon(icon_value->uncompressed);
}
