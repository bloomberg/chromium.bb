// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/fake_sync_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/sync_prefs.h"

namespace syncer {

FakeSyncClient::FakeSyncClient() : factory_(nullptr) {
  // Register sync preferences and set them to "Sync everything" state.
  SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
}

FakeSyncClient::FakeSyncClient(SyncApiComponentFactory* factory)
    : factory_(factory) {
  SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
}

FakeSyncClient::~FakeSyncClient() {}

PrefService* FakeSyncClient::GetPrefService() {
  return &pref_service_;
}

base::FilePath FakeSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

ModelTypeStoreService* FakeSyncClient::GetModelTypeStoreService() {
  return nullptr;
}

DeviceInfoSyncService* FakeSyncClient::GetDeviceInfoSyncService() {
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

sync_sessions::SessionSyncService* FakeSyncClient::GetSessionSyncService() {
  return nullptr;
}

bool FakeSyncClient::HasPasswordStore() {
  return false;
}

base::Closure FakeSyncClient::GetPasswordStateChangedCallback() {
  return base::DoNothing();
}

DataTypeController::TypeVector FakeSyncClient::CreateDataTypeControllers(
    SyncService* sync_service) {
  DCHECK(factory_);
  return factory_->CreateCommonDataTypeControllers(
      /*disabled_types=*/ModelTypeSet(), sync_service);
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

base::WeakPtr<SyncableService> FakeSyncClient::GetSyncableServiceForType(
    ModelType type) {
  return base::WeakPtr<SyncableService>();
}

base::WeakPtr<ModelTypeControllerDelegate>
FakeSyncClient::GetControllerDelegateForModelType(ModelType type) {
  return nullptr;
}

scoped_refptr<ModelSafeWorker> FakeSyncClient::CreateModelWorkerForGroup(
    ModelSafeGroup group) {
  return scoped_refptr<ModelSafeWorker>();
}

SyncApiComponentFactory* FakeSyncClient::GetSyncApiComponentFactory() {
  return factory_;
}

}  // namespace syncer
