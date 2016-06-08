// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/fake_sync_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync_driver/fake_sync_service.h"
#include "components/sync_driver/sync_prefs.h"
#include "sync/util/extensions_activity.h"

namespace sync_driver {

namespace {

void DummyRegisterPlatformTypesCallback(SyncService* sync_service,
                                        syncer::ModelTypeSet,
                                        syncer::ModelTypeSet) {}

}  // namespace

FakeSyncClient::FakeSyncClient()
    : model_type_service_(nullptr),
      factory_(nullptr),
      sync_service_(base::WrapUnique(new FakeSyncService())) {
  // Register sync preferences and set them to "Sync everything" state.
  sync_driver::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  sync_driver::SyncPrefs sync_prefs(GetPrefService());
  sync_prefs.SetFirstSetupComplete();
  sync_prefs.SetKeepEverythingSynced(true);
}

FakeSyncClient::FakeSyncClient(SyncApiComponentFactory* factory)
    : factory_(factory),
      sync_service_(base::WrapUnique(new FakeSyncService())) {
  sync_driver::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
}

FakeSyncClient::~FakeSyncClient() {}

void FakeSyncClient::Initialize() {}

SyncService* FakeSyncClient::GetSyncService() {
  return sync_service_.get();
}

PrefService* FakeSyncClient::GetPrefService() {
  return &pref_service_;
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

syncer_v2::ModelTypeService* FakeSyncClient::GetModelTypeServiceForType(
    syncer::ModelType type) {
  return model_type_service_;
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

void FakeSyncClient::SetModelTypeService(
    syncer_v2::ModelTypeService* model_type_service) {
  model_type_service_ = model_type_service;
}

}  // namespace sync_driver
