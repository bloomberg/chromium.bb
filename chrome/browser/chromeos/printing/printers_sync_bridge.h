// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_SYNC_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace sync_pb {
class PrinterSpecifics;
}

namespace chromeos {

// Moderates interaction with the backing database and integrates with the User
// Sync Service for printers.
class PrintersSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  PrintersSyncBridge(const syncer::ModelTypeStoreFactory& callback,
                     const base::RepeatingClosure& error_callback);
  ~PrintersSyncBridge() override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityDataMap entity_data_map) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllData(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  syncer::ConflictResolution ResolveConflict(
      const syncer::EntityData& local_data,
      const syncer::EntityData& remote_data) const override;

  // Store a new or updated |printer|.
  void AddPrinter(std::unique_ptr<sync_pb::PrinterSpecifics> printer);
  // Remove a printer by |id|.
  bool RemovePrinter(const std::string& id);
  // Returns all printers stored in the database and synced.
  std::vector<sync_pb::PrinterSpecifics> GetAllPrinters() const;
  // Returns the printer with |id| from storage if it could be found.
  base::Optional<sync_pb::PrinterSpecifics> GetPrinter(
      const std::string& id) const;

 private:
  class StoreProxy;

  std::unique_ptr<StoreProxy> store_delegate_;

  // In memory cache of printer information.
  std::map<std::string, std::unique_ptr<sync_pb::PrinterSpecifics>> all_data_;

  DISALLOW_COPY_AND_ASSIGN(PrintersSyncBridge);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_SYNC_BRIDGE_H_
