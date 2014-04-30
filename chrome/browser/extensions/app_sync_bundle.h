// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_SYNC_BUNDLE_H_
#define CHROME_BROWSER_EXTENSIONS_APP_SYNC_BUNDLE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/app_sync_data.h"
#include "chrome/browser/extensions/sync_bundle.h"
#include "sync/api/syncable_service.h"

class ExtensionSyncService;

namespace syncer {
class SyncChangeProcessor;
class SyncErrorFactory;
}

namespace extensions {

class Extension;
class ExtensionSet;

// Bundle of app specific sync stuff.
class AppSyncBundle : public SyncBundle {
 public:
  explicit AppSyncBundle(ExtensionSyncService* extension_sync_service);
  virtual ~AppSyncBundle();

  // Setup this bundle to be sync application data.
  void SetupSync(syncer::SyncChangeProcessor* sync_proccessor,
                 syncer::SyncErrorFactory* sync_error_factory,
                 const syncer::SyncDataList& initial_sync_data);

  // Resets this class back to it default values, which will disable all syncing
  // until a new sync processor is set.
  void Reset();

  // Returns a syncer::SyncChange that will delete the given application.
  syncer::SyncChange CreateSyncChangeToDelete(const Extension* extension) const;

  // Process the sync deletion of the given application.
  void ProcessDeletion(const std::string& extension_id,
                       const syncer::SyncChange& sync_change);

  // Create a sync change based on |sync_data|.
  syncer::SyncChange CreateSyncChange(const syncer::SyncData& sync_data);

  // Get all the sync data contained in this bundle.
  syncer::SyncDataList GetAllSyncData() const;

  // Process the given sync change and apply it.
  void ProcessSyncChange(AppSyncData app_sync_data);

  // Process the list of sync changes.
  void ProcessSyncChangeList(syncer::SyncChangeList sync_change_list);

  // Check to see if the given |id| is either synced or pending to be synced.
  bool HasExtensionId(const std::string& id) const;
  bool HasPendingExtensionId(const std::string& id) const;

  // Add a pending app to be synced.
  void AddPendingApp(const std::string& id,
                     const AppSyncData& app_sync_data);

  // Returns a vector of all the pending sync data.
  std::vector<AppSyncData> GetPendingData() const;

  // Appends sync data objects for every app in |extensions|.
  void GetAppSyncDataListHelper(
      const ExtensionSet& extensions,
      std::vector<extensions::AppSyncData>* sync_data_list) const;

  // Overrides for SyncBundle.
  // Returns true if SetupSync has been called, false otherwise.
  virtual bool IsSyncing() const OVERRIDE;

  // Sync a newly-installed application or change an existing one.
  virtual void SyncChangeIfNeeded(const Extension& extension) OVERRIDE;

 private:
  // Add a synced app.
  void AddApp(const std::string& id);

  // Remove a synced app.
  void RemoveApp(const std::string& id); // make private

  // Change an app from being pending to synced.
  void MarkPendingAppSynced(const std::string& id);

  ExtensionSyncService* extension_sync_service_; // Own us.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  std::set<std::string> synced_apps_;
  std::map<std::string, AppSyncData> pending_sync_data_;

  DISALLOW_COPY_AND_ASSIGN(AppSyncBundle);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_SYNC_BUNDLE_H_
