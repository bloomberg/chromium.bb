// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_API_STUB_MODEL_TYPE_SERVICE_H_
#define COMPONENTS_SYNC_API_STUB_MODEL_TYPE_SERVICE_H_

#include <memory>
#include <string>

#include "components/sync/api/model_type_service.h"

namespace syncer {

// A non-functional implementation of ModelTypeService for
// testing purposes.
class StubModelTypeService : public ModelTypeService {
 public:
  StubModelTypeService();
  explicit StubModelTypeService(
      const ChangeProcessorFactory& change_processor_factory);
  ~StubModelTypeService() override;

  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  SyncError MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityDataMap entity_data_map) override;
  SyncError ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllData(DataCallback callback) override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  void OnChangeProcessorSet() override;

  bool HasChangeProcessor() const;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_API_STUB_MODEL_TYPE_SERVICE_H_
