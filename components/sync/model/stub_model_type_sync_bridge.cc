// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/stub_model_type_sync_bridge.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/fake_model_type_change_processor.h"

namespace syncer {

StubModelTypeSyncBridge::StubModelTypeSyncBridge()
    : StubModelTypeSyncBridge(
          base::Bind(&FakeModelTypeChangeProcessor::Create)) {}

StubModelTypeSyncBridge::StubModelTypeSyncBridge(
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeSyncBridge(change_processor_factory, PREFERENCES) {}

StubModelTypeSyncBridge::~StubModelTypeSyncBridge() {}

std::unique_ptr<MetadataChangeList>
StubModelTypeSyncBridge::CreateMetadataChangeList() {
  return std::unique_ptr<MetadataChangeList>();
}

base::Optional<ModelError> StubModelTypeSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityDataMap entity_data_map) {
  return {};
}

base::Optional<ModelError> StubModelTypeSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  return {};
}

void StubModelTypeSyncBridge::GetData(StorageKeyList storage_keys,
                                      DataCallback callback) {}

void StubModelTypeSyncBridge::GetAllData(DataCallback callback) {}

std::string StubModelTypeSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  return std::string();
}

std::string StubModelTypeSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  return std::string();
}

}  // namespace syncer
