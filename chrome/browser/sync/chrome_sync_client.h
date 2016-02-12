// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__

#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/sync/glue/extensions_activity_monitor.h"
#include "components/sync_driver/sync_client.h"

class Profile;

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStore;
}

namespace sync_driver {
class SyncApiComponentFactory;
class SyncService;
}

namespace browser_sync {

class ChromeSyncClient : public sync_driver::SyncClient {
 public:
  explicit ChromeSyncClient(Profile* profile);
  ~ChromeSyncClient() override;

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

  // Helpers for overriding getters in tests.
  void SetBrowsingDataRemoverObserverForTesting(
      BrowsingDataRemover::Observer* observer);
  void SetSyncApiComponentFactoryForTesting(
      scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory);

 private:
  // Register data types which are enabled on desktop platforms only.
  // |disabled_types| and |enabled_types| correspond only to those types
  // being explicitly disabled/enabled by the command line.
  void RegisterDesktopDataTypes(sync_driver::SyncService* sync_service,
                                syncer::ModelTypeSet disabled_types,
                                syncer::ModelTypeSet enabled_types);

  // Register data types which are enabled on Android platforms only.
  // |disabled_types| and |enabled_types| correspond only to those types
  // being explicitly disabled/enabled by the command line.
  void RegisterAndroidDataTypes(sync_driver::SyncService* sync_service,
                                syncer::ModelTypeSet disabled_types,
                                syncer::ModelTypeSet enabled_types);

  void ClearBrowsingData(base::Time start, base::Time end);

  Profile* const profile_;

  // The sync api component factory in use by this client.
  scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory_;

  // Members that must be fetched on the UI thread but accessed on their
  // respective backend threads.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  scoped_ptr<sync_sessions::SyncSessionsClient> sync_sessions_client_;

  // Generates and monitors the ExtensionsActivity object used by sync.
  ExtensionsActivityMonitor extensions_activity_monitor_;

  // Used in integration tests.
  BrowsingDataRemover::Observer* browsing_data_remover_observer_;

  base::WeakPtrFactory<ChromeSyncClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSyncClient);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
