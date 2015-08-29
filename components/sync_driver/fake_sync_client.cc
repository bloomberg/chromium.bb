// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/fake_sync_client.h"

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/password_manager/core/browser/password_store.h"

namespace sync_driver {

FakeSyncClient::FakeSyncClient() : factory_(nullptr) {}

FakeSyncClient::FakeSyncClient(SyncApiComponentFactory* factory)
    : factory_(factory) {}

FakeSyncClient::~FakeSyncClient() {}

SyncService* FakeSyncClient::GetSyncService() {
  return nullptr;
}

PrefService* FakeSyncClient::GetPrefService() {
  return nullptr;
}

bookmarks::BookmarkModel* FakeSyncClient::GetBookmarkModel() {
  return nullptr;
}

history::HistoryService* FakeSyncClient::GetHistoryService() {
  return nullptr;
}

scoped_refptr<password_manager::PasswordStore>
FakeSyncClient::GetPasswordStore() {
  return scoped_refptr<password_manager::PasswordStore>();
}

autofill::PersonalDataManager* FakeSyncClient::GetPersonalDataManager() {
  return nullptr;
}

scoped_refptr<autofill::AutofillWebDataService>
FakeSyncClient::GetWebDataService() {
  return scoped_refptr<autofill::AutofillWebDataService>();
}

base::WeakPtr<syncer::SyncableService>
FakeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  return base::WeakPtr<syncer::SyncableService>();
}

SyncApiComponentFactory* FakeSyncClient::GetSyncApiComponentFactory() {
  return factory_;
}

}  // namespace sync_driver
