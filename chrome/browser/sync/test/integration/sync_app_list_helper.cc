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
#include "extensions/browser/extension_prefs.h"
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
  SyncAppListHelper* instance = Singleton<SyncAppListHelper>::get();
  instance->SetupIfNecessary(sync_datatype_helper::test());
  return instance;
}

SyncAppListHelper::SyncAppListHelper() : test_(NULL), setup_completed_(false) {}

SyncAppListHelper::~SyncAppListHelper() {}

void SyncAppListHelper::SetupIfNecessary(SyncTest* test) {
  if (setup_completed_) {
    DCHECK_EQ(test, test_);
    return;
  }
  test_ = test;

  for (int i = 0; i < test->num_clients(); ++i) {
    extensions::ExtensionSystem::Get(test_->GetProfile(i))
        ->InitForRegularProfile(true);
  }
  extensions::ExtensionSystem::Get(test_->verifier())
      ->InitForRegularProfile(true);

  setup_completed_ = true;
}

bool SyncAppListHelper::AppListMatchesVerifier(Profile* profile) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  AppListSyncableService* verifier =
      AppListSyncableServiceFactory::GetForProfile(test_->verifier());
  // Note: sync item entries may not exist in verifier, but item lists should
  // match.
  if (service->model()->top_level_item_list()->item_count() !=
      verifier->model()->top_level_item_list()->item_count()) {
    LOG(ERROR) << "Model item count: "
               << service->model()->top_level_item_list()->item_count()
               << " != "
               << verifier->model()->top_level_item_list()->item_count();
    return false;
  }
  bool res = true;
  for (size_t i = 0; i < service->model()->top_level_item_list()->item_count();
       ++i) {
    AppListItem* item1 = service->model()->top_level_item_list()->item_at(i);
    AppListItem* item2 = verifier->model()->top_level_item_list()->item_at(i);
    if (item1->CompareForTest(item2))
      continue;

    LOG(ERROR) << "Item(" << i << "): " << item1->ToDebugString()
               << " != " << item2->ToDebugString();
    size_t index2;
    if (!verifier->model()->top_level_item_list()->FindItemIndex(item1->id(),
                                                                 &index2)) {
      LOG(ERROR) << " Item(" << i << "): " << item1->ToDebugString()
                 << " Not in verifier.";
    } else {
      LOG(ERROR) << " Item(" << i << "): " << item1->ToDebugString()
                 << " Has different verifier index: " << index2;
      item2 = verifier->model()->top_level_item_list()->item_at(index2);
      LOG(ERROR) << " Verifier Item(" << index2
                 << "): " << item2->ToDebugString();
    }
    res = false;
  }
  return res;
}

bool SyncAppListHelper::AllProfilesHaveSameAppListAsVerifier() {
  bool res = true;
  for (int i = 0; i < test_->num_clients(); ++i) {
    if (!AppListMatchesVerifier(test_->GetProfile(i))) {
      LOG(ERROR) << "Profile " << i
                 << " doesn't have the same app list as the verifier profile.";
      res = false;
    }
  }
  if (!res) {
    Profile* verifier = test_->verifier();
    VLOG(1) << "Verifier: "
            << AppListSyncableServiceFactory::GetForProfile(verifier);
    PrintAppList(test_->verifier());
    for (int i = 0; i < test_->num_clients(); ++i) {
      Profile* profile = test_->GetProfile(i);
      VLOG(1) << "Profile: " << i << ": "
              << AppListSyncableServiceFactory::GetForProfile(profile);
      PrintAppList(profile);
    }
  }
  return res;
}

void SyncAppListHelper::MoveApp(Profile* profile, size_t from, size_t to) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  service->model()->top_level_item_list()->MoveItem(from, to);
}

void SyncAppListHelper::MoveAppToFolder(Profile* profile,
                                        size_t index,
                                        const std::string& folder_id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  service->model()->MoveItemToFolder(
      service->model()->top_level_item_list()->item_at(index), folder_id);
}

void SyncAppListHelper::MoveAppFromFolder(Profile* profile,
                                          size_t index_in_folder,
                                          const std::string& folder_id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  AppListFolderItem* folder = service->model()->FindFolderItem(folder_id);
  if (!folder) {
    LOG(ERROR) << "Folder not found: " << folder_id;
    return;
  }
  service->model()->MoveItemToFolder(
      folder->item_list()->item_at(index_in_folder), "");
}

void SyncAppListHelper::CopyOrdinalsToVerifier(Profile* profile,
                                               const std::string& id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  AppListSyncableService* verifier =
      AppListSyncableServiceFactory::GetForProfile(test_->verifier());
  verifier->model()->top_level_item_list()->SetItemPosition(
      verifier->model()->FindItem(id),
      service->model()->FindItem(id)->position());
}

void SyncAppListHelper::PrintAppList(Profile* profile) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  for (size_t i = 0; i < service->model()->top_level_item_list()->item_count();
       ++i) {
    AppListItem* item = service->model()->top_level_item_list()->item_at(i);
    std::string label = base::StringPrintf("Item(%d): ", static_cast<int>(i));
    PrintItem(profile, item, label);
  }
}

void SyncAppListHelper::PrintItem(Profile* profile,
                                  AppListItem* item,
                                  const std::string& label) {
  extensions::AppSorting* s =
      extensions::ExtensionPrefs::Get(profile)->app_sorting();
  std::string id = item->id();
  if (item->GetItemType() == AppListFolderItem::kItemType) {
    VLOG(1) << label << item->ToDebugString();
    AppListFolderItem* folder = static_cast<AppListFolderItem*>(item);
    for (size_t i = 0; i < folder->item_list()->item_count(); ++i) {
      AppListItem* child = folder->item_list()->item_at(i);
      std::string child_label =
          base::StringPrintf(" Folder Item(%d): ", static_cast<int>(i));
      PrintItem(profile, child, child_label);
    }
    return;
  }
  VLOG(1) << label << item->ToDebugString()
          << " Page: " << s->GetPageOrdinal(id).ToDebugString().substr(0, 8)
          << " Item: "
          << s->GetAppLaunchOrdinal(id).ToDebugString().substr(0, 8);
}
