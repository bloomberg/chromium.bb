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
  // TODO(crbug.com/826982): look for (and update) existing AppServiceAppItem's
  // for the update's AppId, instead of always creating new ones.
  //
  // Do we want to have one AppModelBuilder that observes the Cache (and
  // forwards updates such as icon changes on to each AppItem)?? Or should
  // every AppItem be its own observer??

  if (update.ShowInLauncher() != apps::mojom::OptionalBool::kTrue)
    return;

  InsertApp(std::make_unique<AppServiceAppItem>(
      profile(), model_updater(), GetSyncItem(update.AppId()), update));
}
