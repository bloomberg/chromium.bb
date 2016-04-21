// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_DRIVER_FAKE_SYNC_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/sync_driver/sync_client.h"

namespace sync_driver {
class FakeSyncService;

// Fake implementation of SyncClient interface for tests.
class FakeSyncClient : public SyncClient {
 public:
  FakeSyncClient();
  explicit FakeSyncClient(SyncApiComponentFactory* factory);
  ~FakeSyncClient() override;

  void Initialize() override;

  SyncService* GetSyncService() override;
  PrefService* GetPrefService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  base::Closure GetPasswordStateChangedCallback() override;
  sync_driver::SyncApiComponentFactory::RegisterDataTypesMethod
  GetRegisterPlatformTypesCallback() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  BookmarkUndoService* GetBookmarkUndoServiceIfExists() override;
  invalidation::InvalidationService* GetInvalidationService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  sync_sessions::SyncSessionsClient* GetSyncSessionsClient() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  syncer_v2::ModelTypeService* GetModelTypeServiceForType(
      syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group,
      syncer::WorkerLoopDestructionObserver* observer) override;
  SyncApiComponentFactory* GetSyncApiComponentFactory() override;

 private:
  SyncApiComponentFactory* factory_;
  std::unique_ptr<FakeSyncService> sync_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncClient);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_SYNC_CLIENT_H_
