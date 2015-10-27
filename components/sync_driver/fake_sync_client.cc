// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/fake_sync_client.h"

#include "base/bind.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync_driver/fake_sync_service.h"
#include "sync/util/extensions_activity.h"

namespace sync_driver {

FakeSyncClient::FakeSyncClient()
    : factory_(nullptr),
      sync_service_(make_scoped_ptr(new FakeSyncService())) {}

FakeSyncClient::FakeSyncClient(SyncApiComponentFactory* factory)
    : factory_(factory),
      sync_service_(make_scoped_ptr(new FakeSyncService())) {}

FakeSyncClient::~FakeSyncClient() {}

void FakeSyncClient::Initialize(SyncService* sync_service) {}

SyncService* FakeSyncClient::GetSyncService() {
  return sync_service_.get();
}

PrefService* FakeSyncClient::GetPrefService() {
  return nullptr;
}

bookmarks::BookmarkModel* FakeSyncClient::GetBookmarkModel() {
  return nullptr;
}

favicon::FaviconService* FakeSyncClient::GetFaviconService() {
  return nullptr;
}

history::HistoryService* FakeSyncClient::GetHistoryService() {
  return nullptr;
}

scoped_refptr<password_manager::PasswordStore>
FakeSyncClient::GetPasswordStore() {
  return scoped_refptr<password_manager::PasswordStore>();
}

base::Closure FakeSyncClient::GetPasswordStateChangedCallback() {
  return base::Bind(&base::DoNothing);
}

autofill::PersonalDataManager* FakeSyncClient::GetPersonalDataManager() {
  return nullptr;
}

scoped_refptr<autofill::AutofillWebDataService>
FakeSyncClient::GetWebDataService() {
  return scoped_refptr<autofill::AutofillWebDataService>();
}

BookmarkUndoService* FakeSyncClient::GetBookmarkUndoServiceIfExists() {
  return nullptr;
}

scoped_refptr<syncer::ExtensionsActivity>
FakeSyncClient::GetExtensionsActivity() {
  return scoped_refptr<syncer::ExtensionsActivity>();
}

sync_sessions::SyncSessionsClient* FakeSyncClient::GetSyncSessionsClient() {
  return nullptr;
}

base::WeakPtr<syncer::SyncableService>
FakeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  return base::WeakPtr<syncer::SyncableService>();
}

scoped_refptr<syncer::ModelSafeWorker>
FakeSyncClient::CreateModelWorkerForGroup(
    syncer::ModelSafeGroup group,
    syncer::WorkerLoopDestructionObserver* observer) {
  return scoped_refptr<syncer::ModelSafeWorker>();
}

SyncApiComponentFactory* FakeSyncClient::GetSyncApiComponentFactory() {
  return factory_;
}

}  // namespace sync_driver
