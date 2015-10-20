// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "base/version.h"
#include "chrome/browser/extensions/sync_bundle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "sync/api/syncable_service.h"

class ExtensionService;
class Profile;

namespace extensions {
class Extension;
class ExtensionSet;
class ExtensionSyncData;
}  // namespace extensions

// SyncableService implementation responsible for the APPS and EXTENSIONS data
// types, i.e. "proper" apps/extensions (not themes).
class ExtensionSyncService : public syncer::SyncableService,
                             public KeyedService,
                             public extensions::ExtensionRegistryObserver,
                             public extensions::ExtensionPrefsObserver {
 public:
  explicit ExtensionSyncService(Profile* profile);
  ~ExtensionSyncService() override;

  // Convenience function to get the ExtensionSyncService for a BrowserContext.
  static ExtensionSyncService* Get(content::BrowserContext* context);

  // Notifies Sync (if needed) of a newly-installed extension or a change to
  // an existing extension. Call this when you change an extension setting that
  // is synced as part of ExtensionSyncData (e.g. incognito_enabled or
  // all_urls_enabled).
  void SyncExtensionChangeIfNeeded(const extensions::Extension& extension);

  // Returns whether the extension with the given |id| will be re-enabled once
  // it is updated to the given |version|. This happens when we get a Sync
  // update telling us to re-enable a newer version that what is currently
  // installed.
  bool HasPendingReenable(const std::string& id,
                          const base::Version& version) const;

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

  void SetSyncStartFlareForTesting(
      const syncer::SyncableService::StartSyncFlare& flare);

 private:
  FRIEND_TEST_ALL_PREFIXES(TwoClientAppsSyncTest, UnexpectedLaunchType);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDisabledGlobalErrorTest,
                           HigherPermissionsFromSync);

  ExtensionService* extension_service() const;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // extensions::ExtensionPrefsObserver:
  void OnExtensionStateChanged(const std::string& extension_id,
                               bool state) override;
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disabled_reasons) override;

  // Gets the SyncBundle for the given |type|.
  extensions::SyncBundle* GetSyncBundle(syncer::ModelType type);
  const extensions::SyncBundle* GetSyncBundle(syncer::ModelType type) const;

  // Creates the ExtensionSyncData for the given app/extension.
  extensions::ExtensionSyncData CreateSyncData(
      const extensions::Extension& extension) const;

  // Applies the given change coming in from the server to the local state.
  void ApplySyncData(const extensions::ExtensionSyncData& extension_sync_data);

  // Applies the bookmark app specific parts of |extension_sync_data|.
  void ApplyBookmarkAppSyncData(
      const extensions::ExtensionSyncData& extension_sync_data);

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

  // The normal profile associated with this ExtensionSyncService.
  Profile* profile_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver> registry_observer_;
  ScopedObserver<extensions::ExtensionPrefs,
                 extensions::ExtensionPrefsObserver> prefs_observer_;

  extensions::SyncBundle app_sync_bundle_;
  extensions::SyncBundle extension_sync_bundle_;

  // Map from extension id to pending update data. Used for two things:
  // - To send the new version back to the sync server while we're waiting for
  //   an extension to update.
  // - For re-enables, to defer granting permissions until the version matches.
  struct PendingUpdate;
  std::map<std::string, PendingUpdate> pending_updates_;

  // Run()ning tells sync to try and start soon, because syncable changes
  // have started happening. It will cause sync to call us back
  // asynchronously via MergeDataAndStartSyncing as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSyncService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
