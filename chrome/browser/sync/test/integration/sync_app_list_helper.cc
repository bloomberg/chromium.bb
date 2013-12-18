// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/common/extensions/sync_helper.h"
#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model.h"

using app_list::AppListItemList;
using app_list::AppListItemModel;
using app_list::AppListSyncableService;
using app_list::AppListSyncableServiceFactory;

SyncAppListHelper* SyncAppListHelper::GetInstance() {
  SyncAppListHelper* instance = Singleton<SyncAppListHelper>::get();
  instance->SetupIfNecessary(sync_datatype_helper::test());
  return instance;
}

SyncAppListHelper::SyncAppListHelper() : test_(NULL), setup_completed_(false) {
}

SyncAppListHelper::~SyncAppListHelper() {
}

void SyncAppListHelper::SetupIfNecessary(SyncTest* test) {
  if (setup_completed_) {
    DCHECK_EQ(test, test_);
    return;
  }
  test_ = test;

  for (int i = 0; i < test->num_clients(); ++i) {
    extensions::ExtensionSystem::Get(
        test_->GetProfile(i))->InitForRegularProfile(true);
  }
  extensions::ExtensionSystem::Get(
      test_->verifier())->InitForRegularProfile(true);

  setup_completed_ = true;
}

bool SyncAppListHelper::AppListMatchesVerifier(Profile* profile) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  AppListSyncableService* verifier =
      AppListSyncableServiceFactory::GetForProfile(test_->verifier());
  if (service->GetNumSyncItemsForTest() !=
      verifier->GetNumSyncItemsForTest()) {
    LOG(ERROR) << "Sync item count: "
               << service->GetNumSyncItemsForTest()
               << " != " << verifier->GetNumSyncItemsForTest();
    return false;
  }
  if (service->model()->item_list()->item_count() !=
      verifier->model()->item_list()->item_count()) {
    LOG(ERROR) << "Model item count: "
               << service->model()->item_list()->item_count()
               << " != " << verifier->model()->item_list()->item_count();
    return false;
  }
  bool res = true;
  for (size_t i = 0; i < service->model()->item_list()->item_count(); ++i) {
    AppListItemModel* item1 = service->model()->item_list()->item_at(i);
    AppListItemModel* item2 = verifier->model()->item_list()->item_at(i);
    if (item1->CompareForTest(item2))
      continue;

    LOG(ERROR) << "Item(" << i << "): " << item1->ToDebugString()
               << " != " << item2->ToDebugString();
    size_t index2;
    if (!verifier->model()->item_list()->FindItemIndex(item1->id(), &index2)) {
      LOG(ERROR) << " Item(" << i << "): " << item1->ToDebugString()
                 << " Not in verifier.";
    } else {
      LOG(ERROR) << " Item(" << i << "): " << item1->ToDebugString()
                 << " Has different verifier index: " << index2;
      item2 = verifier->model()->item_list()->item_at(index2);
      LOG(ERROR) << " Verifier Item(" << index2 << "): "
                 << item2->ToDebugString();
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
    VLOG(1) << "Verifier:";
    PrintAppList(test_->verifier());
    for (int i = 0; i < test_->num_clients(); ++i) {
      VLOG(1) << "Profile: " << i;
      PrintAppList(test_->GetProfile(i));
    }
  }
  return res;
}

void SyncAppListHelper::MoveApp(Profile* profile, size_t from, size_t to) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  service->model()->item_list()->MoveItem(from, to);
}

void SyncAppListHelper::CopyOrdinalsToVerifier(Profile* profile,
                                               const std::string& id) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  AppListSyncableService* verifier =
      AppListSyncableServiceFactory::GetForProfile(test_->verifier());
  verifier->model()->item_list()->SetItemPosition(
      verifier->model()->item_list()->FindItem(id),
      service->model()->item_list()->FindItem(id)->position());
}

void SyncAppListHelper::PrintAppList(Profile* profile) {
  AppListSyncableService* service =
      AppListSyncableServiceFactory::GetForProfile(profile);
  for (size_t i = 0; i < service->model()->item_list()->item_count(); ++i) {
    AppListItemModel* item = service->model()->item_list()->item_at(i);
    extensions::AppSorting* s =
        extensions::ExtensionSystem::Get(profile)->extension_service()->
        extension_prefs()->app_sorting();
    std::string id = item->id();
    VLOG(1)
        << "Item(" << i << "): " << item->ToDebugString()
        << " Page: " << s->GetPageOrdinal(id).ToDebugString().substr(0, 8)
        << " Item: " << s->GetAppLaunchOrdinal(id).ToDebugString().substr(0, 8);
  }
}
