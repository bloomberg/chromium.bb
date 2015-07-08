// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/pending_enables.h"
#include "chrome/browser/extensions/sync_bundle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "sync/api/syncable_service.h"

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

class ExtensionSyncService : public syncer::SyncableService,
                             public KeyedService {
 public:
  ExtensionSyncService(Profile* profile,
                       extensions::ExtensionPrefs* extension_prefs,
                       ExtensionService* extension_service);

  ~ExtensionSyncService() override;

  // Convenience function to get the ExtensionSyncService for a BrowserContext.
  static ExtensionSyncService* Get(content::BrowserContext* context);

  // Extracts the data needed to sync the uninstall of |extension|, but doesn't
  // actually sync anything now. Call |ProcessSyncUninstallExtension| later with
  // the returned SyncData to actually commit the change.
  syncer::SyncData PrepareToSyncUninstallExtension(
      const extensions::Extension& extension);
  // Commit a sync uninstall that was previously prepared with
  // PrepareToSyncUninstallExtension.
  void ProcessSyncUninstallExtension(const std::string& extension_id,
                                     const syncer::SyncData& sync_data);

  void SyncEnableExtension(const extensions::Extension& extension);
  void SyncDisableExtension(const extensions::Extension& extension);

  void SyncOrderingChange(const std::string& extension_id);

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

  // Creates the ExtensionSyncData for the given app/extension.
  extensions::ExtensionSyncData CreateSyncData(
      const extensions::Extension& extension) const;

  // Applies the change specified passed in by either ExtensionSyncData to the
  // current system.
  // Returns false if the changes were not completely applied and were added
  // to the pending list to be tried again.
  bool ApplySyncData(const extensions::ExtensionSyncData& extension_sync_data);

  // |flare| provides a StartSyncFlare to the SyncableService. See
  // sync_start_util for more. Public for testing.
  void SetSyncStartFlare(const syncer::SyncableService::StartSyncFlare& flare);

 private:
  // Whether the given extension has been enabled before sync has started.
  bool IsPendingEnable(const std::string& extension_id) const;

  // Gets the SyncBundle for the given |type|.
  extensions::SyncBundle* GetSyncBundle(syncer::ModelType type);
  const extensions::SyncBundle* GetSyncBundle(syncer::ModelType type) const;

  // Gets the ExtensionSyncData for all apps or extensions.
  std::vector<extensions::ExtensionSyncData> GetSyncDataList(
      syncer::ModelType type) const;

  void FillSyncDataList(
      const extensions::ExtensionSet& extensions,
      syncer::ModelType type,
      std::vector<extensions::ExtensionSyncData>* sync_data_list) const;

  // Handles applying the extension specific values in |extension_sync_data| to
  // the current system.
  // Returns false if the changes were not completely applied and need to be
  // tried again later.
  bool ApplyExtensionSyncDataHelper(
      const extensions::ExtensionSyncData& extension_sync_data,
      syncer::ModelType type);

  // Processes the bookmark app specific parts of an AppSyncData.
  void ApplyBookmarkAppSyncData(
      const extensions::ExtensionSyncData& extension_sync_data);

  // The normal profile associated with this ExtensionService.
  Profile* profile_;

  // Preferences for the owning profile.
  extensions::ExtensionPrefs* extension_prefs_;

  ExtensionService* extension_service_;

  extensions::SyncBundle app_sync_bundle_;
  extensions::SyncBundle extension_sync_bundle_;

  // Set of extensions/apps that have been enabled before sync has started.
  // TODO(treib,kalman): This seems wrong. Why are enables special, as opposed
  // to disables, or any other changes?
  extensions::PendingEnables pending_app_enables_;
  extensions::PendingEnables pending_extension_enables_;

  // Run()ning tells sync to try and start soon, because syncable changes
  // have started happening. It will cause sync to call us back
  // asynchronously via MergeDataAndStartSyncing as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSyncService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
