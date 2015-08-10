// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/sync_bundle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "sync/api/syncable_service.h"

class ExtensionService;
class Profile;

namespace extensions {
class Extension;
class ExtensionSet;
class ExtensionSyncData;
}  // namespace extensions

namespace syncer {
class SyncChange;
class SyncChangeProcessor;
class SyncErrorFactory;
}

// SyncableService implementation responsible for the APPS and EXTENSIONS data
// types, i.e. "proper" apps/extensions (not themes).
class ExtensionSyncService : public syncer::SyncableService,
                             public KeyedService {
 public:
  explicit ExtensionSyncService(Profile* profile);
  ~ExtensionSyncService() override;

  // Convenience function to get the ExtensionSyncService for a BrowserContext.
  static ExtensionSyncService* Get(content::BrowserContext* context);

  // Notifies Sync that the given |extension| has been uninstalled.
  void SyncUninstallExtension(const extensions::Extension& extension);

  // Notifies Sync (if needed) of a newly-installed extension or a change to
  // an existing extension.
  void SyncExtensionChangeIfNeeded(const extensions::Extension& extension);

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TwoClientAppsSyncTest, UnexpectedLaunchType);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDisabledGlobalErrorTest,
                           HigherPermissionsFromSync);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDisabledGlobalErrorTest, RemoteInstall);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           DeferredSyncStartupPreInstalledComponent);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           DeferredSyncStartupPreInstalledNormal);
  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           DeferredSyncStartupOnInstall);
  friend class EphemeralAppBrowserTest;

  void SetSyncStartFlareForTesting(
      const syncer::SyncableService::StartSyncFlare& flare);

  ExtensionService* extension_service() const;

  // Gets the SyncBundle for the given |type|.
  extensions::SyncBundle* GetSyncBundle(syncer::ModelType type);
  const extensions::SyncBundle* GetSyncBundle(syncer::ModelType type) const;

  // Creates the ExtensionSyncData for the given app/extension.
  extensions::ExtensionSyncData CreateSyncData(
      const extensions::Extension& extension) const;

  // Applies the given change coming in from the server to the local state.
  // Returns false if the changes were not completely applied and were added
  // to the pending list.
  bool ApplySyncData(const extensions::ExtensionSyncData& extension_sync_data);

  // Collects the ExtensionSyncData for all installed apps or extensions.
  // If |include_everything| is true, includes all installed extensions,
  // otherwise only those that have the NeedsSync pref set, i.e. which have
  // local changes that need to be pushed.
  std::vector<extensions::ExtensionSyncData> GetLocalSyncDataList(
      syncer::ModelType type, bool include_everything) const;

  // Helper for GetLocalSyncDataList.
  void FillSyncDataList(
      const extensions::ExtensionSet& extensions,
      syncer::ModelType type,
      bool include_everything,
      std::vector<extensions::ExtensionSyncData>* sync_data_list) const;

  // Handles applying the extension specific values in |extension_sync_data| to
  // the local state.
  // Returns false if the changes were not completely applied.
  bool ApplyExtensionSyncDataHelper(
      const extensions::ExtensionSyncData& extension_sync_data,
      syncer::ModelType type);

  // Processes the bookmark app specific parts of an AppSyncData.
  void ApplyBookmarkAppSyncData(
      const extensions::ExtensionSyncData& extension_sync_data);

  // The normal profile associated with this ExtensionSyncService.
  Profile* profile_;

  extensions::SyncBundle app_sync_bundle_;
  extensions::SyncBundle extension_sync_bundle_;

  // Run()ning tells sync to try and start soon, because syncable changes
  // have started happening. It will cause sync to call us back
  // asynchronously via MergeDataAndStartSyncing as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSyncService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
