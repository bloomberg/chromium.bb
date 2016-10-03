// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/api/stub_model_type_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/api/fake_model_type_change_processor.h"

namespace syncer {

StubModelTypeService::StubModelTypeService()
    : StubModelTypeService(base::Bind(&FakeModelTypeChangeProcessor::Create)) {}

StubModelTypeService::StubModelTypeService(
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeService(change_processor_factory, PREFERENCES) {}

StubModelTypeService::~StubModelTypeService() {}

std::unique_ptr<MetadataChangeList>
StubModelTypeService::CreateMetadataChangeList() {
  return std::unique_ptr<MetadataChangeList>();
}

SyncError StubModelTypeService::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityDataMap entity_data_map) {
  return SyncError();
}

SyncError StubModelTypeService::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  return SyncError();
}

void StubModelTypeService::GetData(StorageKeyList storage_keys,
                                   DataCallback callback) {}

void StubModelTypeService::GetAllData(DataCallback callback) {}

std::string StubModelTypeService::GetClientTag(const EntityData& entity_data) {
  return std::string();
}

std::string StubModelTypeService::GetStorageKey(const EntityData& entity_data) {
  return std::string();
}

void StubModelTypeService::OnChangeProcessorSet() {}

bool StubModelTypeService::HasChangeProcessor() const {
  return change_processor() != nullptr;
}

}  // namespace syncer
