// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
#define CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"

class ExtensionSyncService;

namespace extensions {

class Extension;
class ExtensionSyncData;

class SyncBundle {
 public:
  explicit SyncBundle(ExtensionSyncService* sync_service);
  ~SyncBundle();

  void MergeDataAndStartSyncing(
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor);

  // Resets this class back to its default values, which will disable all
  // syncing until StartSyncing is called again.
  void Reset();

  // Has this bundle started syncing yet?
  // Returns true if MergeDataAndStartSyncing has been called, false otherwise.
  bool IsSyncing() const;

  // Checks if the extension with the given |id| is synced.
  bool HasExtensionId(const std::string& id) const;

  // Whether the given extension should be included in the SyncDataList to be
  // sent to the server. Returns false if there is pending data that should be
  // used instead.
  bool ShouldIncludeInLocalSyncDataList(const Extension& extension) const;

  // Handles the given list of local SyncDatas. This updates the set of synced
  // extensions as appropriate, and then pushes the corresponding SyncChanges
  // to the server.
  void PushSyncDataList(const syncer::SyncDataList& sync_data_list);

  // Handles the sync deletion of the given extension. This updates the set of
  // synced extensions as appropriate, and then pushes a SyncChange to the
  // server.
  void PushSyncDeletion(const std::string& extension_id,
                        const syncer::SyncData& sync_data);

  // Pushes any sync changes to |extension| to the server.
  void PushSyncChangeIfNeeded(const Extension& extension);

  // Applies the given SyncChange coming from the server.
  void ApplySyncChange(const syncer::SyncChange& sync_change);

  // Checks if the extension with the given |id| is pending to be synced.
  bool HasPendingExtensionId(const std::string& id) const;

  // Adds a pending extension to be synced.
  void AddPendingExtension(const std::string& id,
                           const ExtensionSyncData& sync_data);

  // Returns a vector of all the pending sync data.
  std::vector<ExtensionSyncData> GetPendingData() const;

 private:
  // Creates a SyncChange to add or update an extension.
  syncer::SyncChange CreateSyncChange(const std::string& extension_id,
                                      const syncer::SyncData& sync_data) const;

  // Pushes the given list of SyncChanges to the server.
  void PushSyncChanges(const syncer::SyncChangeList& sync_change_list);

  void AddSyncedExtension(const std::string& id);
  void RemoveSyncedExtension(const std::string& id);

  // Changes an extension from being pending to synced.
  void MarkPendingExtensionSynced(const std::string& id);

  ExtensionSyncService* sync_service_;  // Owns us.

  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  std::set<std::string> synced_extensions_;

  std::map<std::string, ExtensionSyncData> pending_sync_data_;

  DISALLOW_COPY_AND_ASSIGN(SyncBundle);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SYNC_BUNDLE_H_
