// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__

#include "components/sync_driver/sync_client.h"

#include "chrome/browser/sync/glue/extensions_activity_monitor.h"

class Profile;

namespace sync_driver {
class SyncApiComponentFactory;
class SyncService;
}

namespace browser_sync {

class ChromeSyncClient : public sync_driver::SyncClient {
 public:
  ChromeSyncClient(
      Profile* profile,
      scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory);
  ~ChromeSyncClient() override;

  // Initializes the ChromeSyncClient internal state.
  void Initialize(sync_driver::SyncService* sync_service) override;

  // SyncClient implementation.
  sync_driver::SyncService* GetSyncService() override;
  PrefService* GetPrefService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  scoped_refptr<password_manager::PasswordStore> GetPasswordStore() override;
  base::Closure GetPasswordStateChangedCallback() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  scoped_refptr<autofill::AutofillWebDataService> GetWebDataService() override;
  BookmarkUndoService* GetBookmarkUndoServiceIfExists() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group,
      syncer::WorkerLoopDestructionObserver* observer) override;
  sync_driver::SyncApiComponentFactory* GetSyncApiComponentFactory() override;

 private:
  Profile* const profile_;

  // The sync api component factory in use by this client.
  scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory_;

  // Members that must be fetched on the UI thread but accessed on their
  // respective backend threads.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // TODO(zea): this is a member only because Typed URLs needs access to
  // the UserShare and Cryptographer outside of the UI thread. Remove this
  // once that's no longer the case.
  // Note: not owned.
  sync_driver::SyncService* sync_service_;

  // Generates and monitors the ExtensionsActivity object used by sync.
  ExtensionsActivityMonitor extensions_activity_monitor_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CLIENT_H__
