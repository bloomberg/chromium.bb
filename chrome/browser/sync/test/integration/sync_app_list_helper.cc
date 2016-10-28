// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

using app_list::AppListFolderItem;
using app_list::AppListItem;
using app_list::AppListItemList;
using app_list::AppListSyncableService;
using app_list::AppListSyncableServiceFactory;

SyncAppListHelper* SyncAppListHelper::GetInstance() {
  SyncAppListHelper* instance = base::Singleton<SyncAppListHelper>::get();
  instance->SetupIfNecessary(sync_datatype_helper::test());
  return instance;
}

SyncAppListHelper::SyncAppListHelper()
    : test_(nullptr), setup_completed_(false) {}

SyncAppListHelper::~SyncAppListHelper() {}

void SyncAppListHelper::SetupIfNecessary(SyncTest* test) {
  if (setup_completed_) {
    DCHECK_EQ(test, test_);
    return;
  }
  test_ = test;
  for (auto* profile : test_->GetAllProfiles()) {
    extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(true);
  }

  setup_completed_ = true;
}

bool SyncAppListHelper::AppListMatch(Profile* profile1, Profile* profile2) {
  AppListSyncableService* service1 =
      AppListSyncableServiceFactory::GetForProfile(profile1);
  AppListSyncableService* service2 =
      AppListSyncableServiceFactory::GetForProfile(profile2);
  // Note: sync item entries may not exist in verifier, but item lists should
  // match.
  if (service1->GetModel()->top_level_item_list()->item_count() !=
      service2->GetModel()->top_level_item_list()->item_count()) {
    LOG(ERROR) << "Model item count: "
               << service1->GetModel()->top_level_item_list()->item_count()
               << " != "
               << service2->GetModel()->top_level_item_list()->item_count();
    return false;
  }
  bool res = true;
  for (size_t i = 0;
       i < service1->GetModel()->top_level_item_list()->item_count(); ++i) {
    AppListItem* item1 =
        service1->GetModel()->top_level_item_list()->item_at(i);
    AppListItem* item2 =
        service2->GetModel()->top_level_item_list()->item_at(i);
    if (item1->CompareForTest(item2))
      continue;

    LOG(ERROR) << "Item(" << i << "): " << item1->ToDebugString()
               << " != " << item2->ToDebugString();
    size_t index2;
    if (!service2->GetModel()->top_level_item_list()->FindItemIndex(item1->id(),
                                                                    &index2)) {
      LOG(ERROR) << " Item(" << i << "): " << item1->ToDebugString()
                 << " Not in profile2.";
    } else {
      LOG(ERROR) << " Item(" << i << "): " << item1->ToDebugString()
                 << " Has different profile2 index: " << index2;
      item2 = service2->GetModel()->top_level_item_list()->item_at(index2);
      LOG(ERROR) << " profile2 Item(" << index2
                 << "): " << item2->ToDebugString();
    }
    res = false;
  }
  return res;
}

bool SyncAppListHelper::AllProfilesHaveSameAppList() {
  const auto& profiles = test_->GetAllProfiles();
  for (auto* profile : profiles) {
    if (profile != profiles.front() &&
        !AppListMatch(profiles.front(), profile)) {
      DVLOG(1) << "Profile1: "
               << AppListSyncableServiceFactory::GetForProfile(profile);
      PrintAppList(profile);
      DVLOG(1) << "Profile2: " <<
          AppListSyncableServiceFactory::GetForProfile(profiles.front());
      PrintAppList(profiles.front());
      return false;
    }
  }
  return true;
}

void SyncAppListHelper::MoveApp(Profile* profile, size_t from, size_t to) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  service->GetModel()->top_level_item_list()->MoveItem(from, to);
}

void SyncAppListHelper::MoveAppToFolder(Profile* profile,
                                        size_t index,
                                        const std::string& folder_id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  service->GetModel()->MoveItemToFolder(
      service->GetModel()->top_level_item_list()->item_at(index), folder_id);
}

void SyncAppListHelper::MoveAppFromFolder(Profile* profile,
                                          size_t index_in_folder,
                                          const std::string& folder_id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  AppListFolderItem* folder = service->GetModel()->FindFolderItem(folder_id);
  if (!folder) {
    LOG(ERROR) << "Folder not found: " << folder_id;
    return;
  }
  service->GetModel()->MoveItemToFolder(
      folder->item_list()->item_at(index_in_folder), "");
}

void SyncAppListHelper::PrintAppList(Profile* profile) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  for (size_t i = 0;
       i < service->GetModel()->top_level_item_list()->item_count(); ++i) {
    AppListItem* item = service->GetModel()->top_level_item_list()->item_at(i);
    std::string label = base::StringPrintf("Item(%d): ", static_cast<int>(i));
    PrintItem(profile, item, label);
  }
}

void SyncAppListHelper::PrintItem(Profile* profile,
                                  AppListItem* item,
                                  const std::string& label) {
  extensions::AppSorting* s =
      extensions::ExtensionSystem::Get(profile)->app_sorting();
  std::string id = item->id();
  if (item->GetItemType() == AppListFolderItem::kItemType) {
    DVLOG(1) << label << item->ToDebugString();
    AppListFolderItem* folder = static_cast<AppListFolderItem*>(item);
    for (size_t i = 0; i < folder->item_list()->item_count(); ++i) {
      AppListItem* child = folder->item_list()->item_at(i);
      std::string child_label =
          base::StringPrintf(" Folder Item(%d): ", static_cast<int>(i));
      PrintItem(profile, child, child_label);
    }
    return;
  }
  DVLOG(1) << label << item->ToDebugString()
           << " Page: " << s->GetPageOrdinal(id).ToDebugString().substr(0, 8)
           << " Item: "
           << s->GetAppLaunchOrdinal(id).ToDebugString().substr(0, 8);
}
