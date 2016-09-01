// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_model_builder.h"

#include <utility>

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

AppListModelBuilder::AppListModelBuilder(AppListControllerDelegate* controller,
                                         const char* item_type)
    : controller_(controller),
      item_type_(item_type) {
}

AppListModelBuilder::~AppListModelBuilder() {
  if (!service_)
    model_->top_level_item_list()->RemoveObserver(this);
}

void AppListModelBuilder::InitializeWithService(
    app_list::AppListSyncableService* service,
    app_list::AppListModel* model) {
  DCHECK(!service_ && !profile_);
  model_ = model;
  service_ = service;
  profile_ = service->profile();

  BuildModel();
}

void AppListModelBuilder::InitializeWithProfile(Profile* profile,
                                                app_list::AppListModel* model) {
  DCHECK(!service_ && !profile_);
  model_ = model;
  model_->top_level_item_list()->AddObserver(this);
  profile_ = profile;

  BuildModel();
}

void AppListModelBuilder::InsertApp(
    std::unique_ptr<app_list::AppListItem> app) {
  if (service_) {
    service_->AddItem(std::move(app));
    return;
  }
  model_->AddItem(std::move(app));
}

void AppListModelBuilder::RemoveApp(const std::string& id,
                                    bool unsynced_change) {
  if (!unsynced_change && service_) {
    service_->RemoveUninstalledItem(id);
    return;
  }
  model_->DeleteUninstalledItem(id);
}

const app_list::AppListSyncableService::SyncItem*
    AppListModelBuilder::GetSyncItem(const std::string& id) {
  return service_ ? service_->GetSyncItem(id) : nullptr;
}

app_list::AppListItem* AppListModelBuilder::GetAppItem(const std::string& id) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item && item->GetItemType() != item_type_) {
    VLOG(2) << "App Item matching id: " << id
            << " has incorrect type: '" << item->GetItemType() << "'";
    return nullptr;
  }
  return item;
}
