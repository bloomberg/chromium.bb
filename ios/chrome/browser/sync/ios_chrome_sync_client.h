// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__
#define IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_driver/sync_client.h"

namespace autofill {
class AutofillWebDataService;
}

namespace ios {
class ChromeBrowserState;
}

namespace password_manager {
class PasswordStore;
}

namespace sync_driver {
class SyncApiComponentFactory;
class SyncService;
}

class IOSChromeSyncClient : public sync_driver::SyncClient {
 public:
  explicit IOSChromeSyncClient(ios::ChromeBrowserState* browser_state);
  ~IOSChromeSyncClient() override;

  // SyncClient implementation.
  void Initialize() override;
  sync_driver::SyncService* GetSyncService() override;
  PrefService* GetPrefService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  sync_driver::ClearBrowsingDataCallback GetClearBrowsingDataCallback()
      override;
  base::Closure GetPasswordStateChangedCallback() override;
  sync_driver::SyncApiComponentFactory::RegisterDataTypesMethod
  GetRegisterPlatformTypesCallback() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  invalidation::InvalidationService* GetInvalidationService() override;
  BookmarkUndoService* GetBookmarkUndoServiceIfExists() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  sync_sessions::SyncSessionsClient* GetSyncSessionsClient() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  base::WeakPtr<syncer_v2::ModelTypeService> GetModelTypeServiceForType(
      syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group,
      syncer::WorkerLoopDestructionObserver* observer) override;
  sync_driver::SyncApiComponentFactory* GetSyncApiComponentFactory() override;

  void SetSyncApiComponentFactoryForTesting(
      scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory);

 private:
  void ClearBrowsingData(base::Time start, base::Time end);

  ios::ChromeBrowserState* const browser_state_;

  // The sync api component factory in use by this client.
  scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory_;

  // Members that must be fetched on the UI thread but accessed on their
  // respective backend threads.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  scoped_ptr<sync_sessions::SyncSessionsClient> sync_sessions_client_;

  const scoped_refptr<syncer::ExtensionsActivity> dummy_extensions_activity_;

  base::WeakPtrFactory<IOSChromeSyncClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeSyncClient);
};

#endif  // IOS_CHROME_BROWSER_SYNC_IOS_CHROME_SYNC_CLIENT_H__
