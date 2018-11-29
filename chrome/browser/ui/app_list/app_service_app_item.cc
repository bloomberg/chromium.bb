// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service_app_item.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_item.h"

// static
const char AppServiceAppItem::kItemType[] = "AppServiceAppItem";

AppServiceAppItem::AppServiceAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const apps::AppUpdate& app_update)
    : ChromeAppListItem(profile, app_update.AppId()),
      app_type_(app_update.AppType()) {
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

void AppServiceAppItem::Activate(int event_flags) {
  // This code snippet makes us update the same UMA histogram both before and
  // after --enable-features=AppService. Before, it was updated in
  // internal_app_item.h, which meant that there were histogram entries:
  //   - only for internal apps, not for other app types.
  //   - only when launched from the app list, and not from e.g. the shelf.
  //     Keeping this behavior means that, even after the App Service is
  //     enabled, the histogram is updated in this UI code rather than by the
  //     App Service's app publisher, since the publisher can also be invoked
  //     from other UIs like the shelf.
  //
  // TODO(crbug.com/826982): be more structured about UMA histograms, after we
  // migrate more of the ui/app_list code over to use the App Service. Should
  // they apply to all app types? Should they apply to all the different ways
  // we can launch apps? Should we just delete the
  // UMA_HISTOGRAM_ENUMERATION("Apps.AppListInternalApp.Activate", etc) code?
  if (app_type_ == apps::mojom::AppType::kBuiltIn)
    InternalAppItem::RecordActiveHistogram(id());

  apps::AppServiceProxy* proxy = apps::AppServiceProxy::Get(profile());
  if (proxy)
    proxy->Launch(id(), event_flags);
}

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
