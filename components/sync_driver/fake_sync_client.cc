// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/fake_sync_client.h"

#include "base/bind.h"
#include "components/sync_driver/fake_sync_service.h"
#include "sync/util/extensions_activity.h"

namespace sync_driver {

namespace {

void DummyClearBrowsingDataCallback(base::Time start, base::Time end) {}

void DummyRegisterPlatformTypesCallback(SyncService* sync_service,
                                        syncer::ModelTypeSet,
                                        syncer::ModelTypeSet) {}

}  // namespace

FakeSyncClient::FakeSyncClient()
    : factory_(nullptr),
      sync_service_(make_scoped_ptr(new FakeSyncService())) {}

FakeSyncClient::FakeSyncClient(SyncApiComponentFactory* factory)
    : factory_(factory),
      sync_service_(make_scoped_ptr(new FakeSyncService())) {}

FakeSyncClient::~FakeSyncClient() {}

void FakeSyncClient::Initialize() {}

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

ClearBrowsingDataCallback FakeSyncClient::GetClearBrowsingDataCallback() {
  return base::Bind(&DummyClearBrowsingDataCallback);
}

base::Closure FakeSyncClient::GetPasswordStateChangedCallback() {
  return base::Bind(&base::DoNothing);
}

sync_driver::SyncApiComponentFactory::RegisterDataTypesMethod
FakeSyncClient::GetRegisterPlatformTypesCallback() {
  return base::Bind(&DummyRegisterPlatformTypesCallback);
}

autofill::PersonalDataManager* FakeSyncClient::GetPersonalDataManager() {
  return nullptr;
}

BookmarkUndoService* FakeSyncClient::GetBookmarkUndoServiceIfExists() {
  return nullptr;
}

invalidation::InvalidationService* FakeSyncClient::GetInvalidationService() {
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

base::WeakPtr<syncer_v2::ModelTypeService>
FakeSyncClient::GetModelTypeServiceForType(syncer::ModelType type) {
  return base::WeakPtr<syncer_v2::ModelTypeService>();
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
