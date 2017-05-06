// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {

class UserEventSyncBridge : public ModelTypeSyncBridge {
 public:
  UserEventSyncBridge(const ModelTypeStoreFactory& store_factory,
                      const ChangeProcessorFactory& change_processor_factory);
  ~UserEventSyncBridge() override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  base::Optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityDataMap entity_data_map) override;
  base::Optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllData(DataCallback callback) override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  void DisableSync() override;

  void RecordUserEvent(std::unique_ptr<sync_pb::UserEventSpecifics> specifics);

 private:
  void OnStoreCreated(ModelTypeStore::Result result,
                      std::unique_ptr<ModelTypeStore> store);
  void OnReadAllMetadata(base::Optional<ModelError> error,
                         std::unique_ptr<MetadataBatch> metadata_batch);
  void OnCommit(ModelTypeStore::Result result);
  void OnReadData(DataCallback callback,
                  ModelTypeStore::Result result,
                  std::unique_ptr<ModelTypeStore::RecordList> data_records,
                  std::unique_ptr<ModelTypeStore::IdList> missing_id_list);
  void OnReadAllData(DataCallback callback,
                     ModelTypeStore::Result result,
                     std::unique_ptr<ModelTypeStore::RecordList> data_records);
  void OnReadAllDataToDelete(
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> data_records);

  // Persistent storage for in flight events. Should remain quite small, as we
  // delete upon commit confirmation.
  std::unique_ptr<ModelTypeStore> store_;

  DISALLOW_COPY_AND_ASSIGN(UserEventSyncBridge);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_
