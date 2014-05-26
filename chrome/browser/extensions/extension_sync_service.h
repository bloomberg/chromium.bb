// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/app_sync_bundle.h"
#include "chrome/browser/extensions/extension_sync_bundle.h"
#include "chrome/browser/extensions/pending_enables.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "sync/api/string_ordinal.h"
#include "sync/api/sync_change.h"
#include "sync/api/syncable_service.h"

class ExtensionSyncData;
class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace extensions {
class AppSyncData;
class ExtensionPrefs;
class ExtensionSyncData;
}  // namespace extensions

namespace syncer {
class SyncErrorFactory;
}

class ExtensionSyncService : public syncer::SyncableService,
                             public KeyedService {
 public:
  ExtensionSyncService(Profile* profile,
                       extensions::ExtensionPrefs* extension_prefs,
                       ExtensionService* extension_service);

  virtual ~ExtensionSyncService();

  // Convenience function to get the ExtensionSyncService for a Profile.
  static ExtensionSyncService* Get(Profile* profile);

  const extensions::ExtensionPrefs& extension_prefs() const {
    return *extension_prefs_;
  }

  // Notifies Sync (if needed) of a newly-installed extension or a change to
  // an existing extension.
  virtual void SyncExtensionChangeIfNeeded(
      const extensions::Extension& extension);

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // Gets the sync data for the given extension, assuming that the extension is
  // syncable.
  extensions::ExtensionSyncData GetExtensionSyncData(
      const extensions::Extension& extension) const;

  // Gets the sync data for the given app, assuming that the app is
  // syncable.
  extensions::AppSyncData GetAppSyncData(
      const extensions::Extension& extension) const;

  // Gets the ExtensionSyncData for all extensions.
  std::vector<extensions::ExtensionSyncData> GetExtensionSyncDataList() const;

  // Gets the AppSyncData for all extensions.
  std::vector<extensions::AppSyncData> GetAppSyncDataList() const;

  // Applies the change specified passed in by either ExtensionSyncData or
  // AppSyncData to the current system.
  // Returns false if the changes were not completely applied and were added
  // to the pending list to be tried again.
  bool ProcessExtensionSyncData(
      const extensions::ExtensionSyncData& extension_sync_data);
  bool ProcessAppSyncData(const extensions::AppSyncData& app_sync_data);

  // Processes the bookmark app specific parts of an AppSyncData.
  void ProcessBookmarkAppSyncData(const extensions::AppSyncData& app_sync_data);

  syncer::SyncChange PrepareToSyncUninstallExtension(
      const extensions::Extension* extension,
      bool extensions_ready);
  void ProcessSyncUninstallExtension(const std::string& extension_id,
                                     const syncer::SyncChange& sync_change);

  void SyncEnableExtension(const extensions::Extension& extension);
  void SyncDisableExtension(const extensions::Extension& extension);

  void SyncOrderingChange(const std::string& extension_id);

  // |flare| provides a StartSyncFlare to the SyncableService. See
  // sync_start_util for more.
  void SetSyncStartFlare(const syncer::SyncableService::StartSyncFlare& flare);

  Profile* profile() { return profile_; }

 private:
  // Return true if the sync type of |extension| matches |type|.
  bool IsCorrectSyncType(const extensions::Extension& extension,
                         syncer::ModelType type)
      const;

  // Whether the given extension has been enabled before sync has started.
  bool IsPendingEnable(const std::string& extension_id) const;

  // Handles setting the extension specific values in |extension_sync_data| to
  // the current system.
  // Returns false if the changes were not completely applied and need to be
  // tried again later.
  bool ProcessExtensionSyncDataHelper(
      const extensions::ExtensionSyncData& extension_sync_data,
      syncer::ModelType type);

  // The normal profile associated with this ExtensionService.
  Profile* profile_;

  // Preferences for the owning profile.
  extensions::ExtensionPrefs* extension_prefs_;

  ExtensionService* extension_service_;

  extensions::AppSyncBundle app_sync_bundle_;
  extensions::ExtensionSyncBundle extension_sync_bundle_;

  // Set of extensions/apps that have been enabled before sync has started.
  extensions::PendingEnables pending_app_enables_;
  extensions::PendingEnables pending_extension_enables_;

  // Sequenced task runner for extension related file operations.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Run()ning tells sync to try and start soon, because syncable changes
  // have started happening. It will cause sync to call us back
  // asynchronously via MergeDataAndStartSyncing as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSyncService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_SERVICE_H_
