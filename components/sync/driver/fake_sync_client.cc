// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/fake_sync_client.h"

#include "base/bind.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/fake_sync_service.h"

namespace syncer {

namespace {

void DummyRegisterPlatformTypesCallback(SyncService* sync_service,
                                        ModelTypeSet,
                                        ModelTypeSet) {}

}  // namespace

FakeSyncClient::FakeSyncClient()
    : bridge_(nullptr),
      factory_(nullptr),
      sync_service_(std::make_unique<FakeSyncService>()) {
  // Register sync preferences and set them to "Sync everything" state.
  SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  SyncPrefs sync_prefs(GetPrefService());
  sync_prefs.SetFirstSetupComplete();
  sync_prefs.SetKeepEverythingSynced(true);
}

FakeSyncClient::FakeSyncClient(SyncApiComponentFactory* factory)
    : factory_(factory), sync_service_(std::make_unique<FakeSyncService>()) {
  SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
}

FakeSyncClient::~FakeSyncClient() {}

void FakeSyncClient::Initialize() {}

SyncService* FakeSyncClient::GetSyncService() {
  return sync_service_.get();
}

PrefService* FakeSyncClient::GetPrefService() {
  return &pref_service_;
}

base::FilePath FakeSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
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

bool FakeSyncClient::HasPasswordStore() {
  return false;
}

base::Closure FakeSyncClient::GetPasswordStateChangedCallback() {
  return base::Bind(&base::DoNothing);
}

SyncApiComponentFactory::RegisterDataTypesMethod
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

scoped_refptr<ExtensionsActivity> FakeSyncClient::GetExtensionsActivity() {
  return scoped_refptr<ExtensionsActivity>();
}

sync_sessions::SyncSessionsClient* FakeSyncClient::GetSyncSessionsClient() {
  return nullptr;
}

base::WeakPtr<SyncableService> FakeSyncClient::GetSyncableServiceForType(
    ModelType type) {
  return base::WeakPtr<SyncableService>();
}

base::WeakPtr<ModelTypeSyncBridge> FakeSyncClient::GetSyncBridgeForModelType(
    ModelType type) {
  return bridge_->AsWeakPtr();
}

scoped_refptr<ModelSafeWorker> FakeSyncClient::CreateModelWorkerForGroup(
    ModelSafeGroup group) {
  return scoped_refptr<ModelSafeWorker>();
}

SyncApiComponentFactory* FakeSyncClient::GetSyncApiComponentFactory() {
  return factory_;
}

void FakeSyncClient::SetModelTypeSyncBridge(ModelTypeSyncBridge* bridge) {
  bridge_ = bridge;
}

}  // namespace syncer
