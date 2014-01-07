// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_BUNDLE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_BUNDLE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_sync_data.h"
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

// Bundle of extension specific sync stuff.
class ExtensionSyncBundle : public SyncBundle {
 public:
  explicit ExtensionSyncBundle(ExtensionSyncService* extension_sync_service);
  virtual ~ExtensionSyncBundle();

  // Setup this bundle to be sync extension data.
  void SetupSync(syncer::SyncChangeProcessor* sync_processor,
                 syncer::SyncErrorFactory* sync_error_factory,
                 const syncer::SyncDataList& initial_sync_data);

  // Resets this class back to it default values, which will disable all syncing
  // until a new sync processor is set.
  void Reset();

  // Returns a syncer::SyncChange that will delete the given extension.
  syncer::SyncChange CreateSyncChangeToDelete(const Extension* extension) const;

  // Process the sync deletion of the given extension.
  void ProcessDeletion(
      std::string extension_id, const syncer::SyncChange& sync_change);

  // Create a sync change based on |sync_data|.
  syncer::SyncChange CreateSyncChange(const syncer::SyncData& sync_data);

  // Get all the sync data contained in this bundle.
  syncer::SyncDataList GetAllSyncData() const;

  // Process the given sync change and apply it.
  void ProcessSyncChange(ExtensionSyncData extension_sync_data);

  // Process the list of sync changes.
  void ProcessSyncChangeList(syncer::SyncChangeList sync_change_list);

  // Check to see if the given |id| is either synced or pending to be synced.
  bool HasExtensionId(const std::string& id) const;
  bool HasPendingExtensionId(const std::string& id) const;

  // Add a pending extension to be synced.
  void AddPendingExtension(const std::string& id,
                           const ExtensionSyncData& extension_sync_data);

  // Returns a vector of all the pending sync data.
  std::vector<ExtensionSyncData> GetPendingData() const;

  // Appends sync data objects for every extension in |extensions|.
  void GetExtensionSyncDataListHelper(
      const ExtensionSet& extensions,
      std::vector<extensions::ExtensionSyncData>* sync_data_list) const;

  // Overrides for SyncBundle.
  // Returns true if SetupSync has been called, false otherwise.
  virtual bool IsSyncing() const OVERRIDE;

  // Sync a newly-installed extension or change an existing one.
  virtual void SyncChangeIfNeeded(const Extension& extension) OVERRIDE;

 private:
  // Add a synced extension.
  void AddExtension(const std::string& id);

  // Remove a synced extension.
  void RemoveExtension(const std::string& id);

  // Change an extension from being pending to synced.
  void MarkPendingExtensionSynced(const std::string& id);

  ExtensionSyncService* extension_sync_service_;  // Owns us.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  std::set<std::string> synced_extensions_;
  std::map<std::string, ExtensionSyncData> pending_sync_data_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSyncBundle);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_BUNDLE_H_
