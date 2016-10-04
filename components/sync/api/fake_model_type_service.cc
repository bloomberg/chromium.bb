// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/api/fake_model_type_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/core/data_batch_impl.h"
#include "components/sync/core/simple_metadata_change_list.h"
#include "components/sync/syncable/syncable_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::EntitySpecifics;
using sync_pb::EntityMetadata;
using sync_pb::ModelTypeState;

namespace syncer {

namespace {

// It is intentionally very difficult to copy an EntityData, as in normal code
// we never want to. However, since we store the data as an EntityData for the
// test code here, this function is needed to manually copy it.
std::unique_ptr<EntityData> CopyEntityData(const EntityData& old_data) {
  std::unique_ptr<EntityData> new_data(new EntityData());
  new_data->id = old_data.id;
  new_data->client_tag_hash = old_data.client_tag_hash;
  new_data->non_unique_name = old_data.non_unique_name;
  new_data->specifics = old_data.specifics;
  new_data->creation_time = old_data.creation_time;
  new_data->modification_time = old_data.modification_time;
  return new_data;
}

}  // namespace

// static
std::string FakeModelTypeService::ClientTagFromKey(const std::string& key) {
  return "ClientTag_" + key;
}

// static
std::string FakeModelTypeService::TagHashFromKey(const std::string& key) {
  return syncable::GenerateSyncableHash(
      PREFERENCES, FakeModelTypeService::ClientTagFromKey(key));
}

// static
EntitySpecifics FakeModelTypeService::GenerateSpecifics(
    const std::string& key,
    const std::string& value) {
  EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(key);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// static
std::unique_ptr<EntityData> FakeModelTypeService::GenerateEntityData(
    const std::string& key,
    const std::string& value) {
  std::unique_ptr<EntityData> entity_data = base::MakeUnique<EntityData>();
  entity_data->client_tag_hash = TagHashFromKey(key);
  entity_data->specifics = GenerateSpecifics(key, value);
  entity_data->non_unique_name = key;
  return entity_data;
}

FakeModelTypeService::Store::Store() {}
FakeModelTypeService::Store::~Store() {}

void FakeModelTypeService::Store::PutData(const std::string& key,
                                          const EntityData& data) {
  data_change_count_++;
  data_store_[key] = CopyEntityData(data);
}

void FakeModelTypeService::Store::PutMetadata(const std::string& key,
                                              const EntityMetadata& metadata) {
  metadata_change_count_++;
  metadata_store_[key] = metadata;
}

void FakeModelTypeService::Store::RemoveData(const std::string& key) {
  data_change_count_++;
  data_store_.erase(key);
}

void FakeModelTypeService::Store::RemoveMetadata(const std::string& key) {
  metadata_change_count_++;
  metadata_store_.erase(key);
}

bool FakeModelTypeService::Store::HasData(const std::string& key) const {
  return data_store_.find(key) != data_store_.end();
}

bool FakeModelTypeService::Store::HasMetadata(const std::string& key) const {
  return metadata_store_.find(key) != metadata_store_.end();
}

const EntityData& FakeModelTypeService::Store::GetData(
    const std::string& key) const {
  return *data_store_.find(key)->second;
}

const std::string& FakeModelTypeService::Store::GetValue(
    const std::string& key) const {
  return GetData(key).specifics.preference().value();
}

const sync_pb::EntityMetadata& FakeModelTypeService::Store::GetMetadata(
    const std::string& key) const {
  return metadata_store_.find(key)->second;
}

std::unique_ptr<MetadataBatch>
FakeModelTypeService::Store::CreateMetadataBatch() const {
  std::unique_ptr<MetadataBatch> metadata_batch(new MetadataBatch());
  metadata_batch->SetModelTypeState(model_type_state_);
  for (const auto& kv : metadata_store_) {
    metadata_batch->AddMetadata(kv.first, kv.second);
  }
  return metadata_batch;
}

void FakeModelTypeService::Store::Reset() {
  data_change_count_ = 0;
  metadata_change_count_ = 0;
  data_store_.clear();
  metadata_store_.clear();
  model_type_state_.Clear();
}

FakeModelTypeService::FakeModelTypeService(
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeService(change_processor_factory, PREFERENCES) {}

FakeModelTypeService::~FakeModelTypeService() {
  CheckPostConditions();
}

EntitySpecifics FakeModelTypeService::WriteItem(const std::string& key,
                                                const std::string& value) {
  std::unique_ptr<EntityData> entity_data = GenerateEntityData(key, value);
  EntitySpecifics specifics_copy = entity_data->specifics;
  WriteItem(key, std::move(entity_data));
  return specifics_copy;
}

// Overloaded form to allow passing of custom entity data.
void FakeModelTypeService::WriteItem(const std::string& key,
                                     std::unique_ptr<EntityData> entity_data) {
  db_.PutData(key, *entity_data);
  if (change_processor()) {
    std::unique_ptr<MetadataChangeList> change_list(
        new SimpleMetadataChangeList());
    change_processor()->Put(key, std::move(entity_data), change_list.get());
    ApplyMetadataChangeList(std::move(change_list));
  }
}

void FakeModelTypeService::DeleteItem(const std::string& key) {
  db_.RemoveData(key);
  if (change_processor()) {
    std::unique_ptr<MetadataChangeList> change_list(
        new SimpleMetadataChangeList());
    change_processor()->Delete(key, change_list.get());
    ApplyMetadataChangeList(std::move(change_list));
  }
}

std::unique_ptr<MetadataChangeList>
FakeModelTypeService::CreateMetadataChangeList() {
  return std::unique_ptr<MetadataChangeList>(new SimpleMetadataChangeList());
}

SyncError FakeModelTypeService::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_changes,
    EntityDataMap data_map) {
  if (service_error_.IsSet()) {
    SyncError error = service_error_;
    service_error_ = SyncError();
    return error;
  }
  // Commit any local entities that aren't being overwritten by the server.
  for (const auto& kv : db_.all_data()) {
    if (data_map.find(kv.first) == data_map.end()) {
      change_processor()->Put(kv.first, CopyEntityData(*kv.second),
                              metadata_changes.get());
    }
  }
  // Store any new remote entities.
  for (const auto& kv : data_map) {
    db_.PutData(kv.first, kv.second.value());
  }
  ApplyMetadataChangeList(std::move(metadata_changes));
  return SyncError();
}

SyncError FakeModelTypeService::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_changes,
    EntityChangeList entity_changes) {
  if (service_error_.IsSet()) {
    SyncError error = service_error_;
    service_error_ = SyncError();
    return error;
  }
  for (const EntityChange& change : entity_changes) {
    switch (change.type()) {
      case EntityChange::ACTION_ADD:
        EXPECT_FALSE(db_.HasData(change.storage_key()));
        db_.PutData(change.storage_key(), change.data());
        break;
      case EntityChange::ACTION_UPDATE:
        EXPECT_TRUE(db_.HasData(change.storage_key()));
        db_.PutData(change.storage_key(), change.data());
        break;
      case EntityChange::ACTION_DELETE:
        EXPECT_TRUE(db_.HasData(change.storage_key()));
        db_.RemoveData(change.storage_key());
        break;
    }
  }
  ApplyMetadataChangeList(std::move(metadata_changes));
  return SyncError();
}

void FakeModelTypeService::ApplyMetadataChangeList(
    std::unique_ptr<MetadataChangeList> change_list) {
  DCHECK(change_list);
  SimpleMetadataChangeList* changes =
      static_cast<SimpleMetadataChangeList*>(change_list.get());
  const auto& metadata_changes = changes->GetMetadataChanges();
  for (const auto& kv : metadata_changes) {
    switch (kv.second.type) {
      case SimpleMetadataChangeList::UPDATE:
        db_.PutMetadata(kv.first, kv.second.metadata);
        break;
      case SimpleMetadataChangeList::CLEAR:
        EXPECT_TRUE(db_.HasMetadata(kv.first));
        db_.RemoveMetadata(kv.first);
        break;
    }
  }
  if (changes->HasModelTypeStateChange()) {
    const SimpleMetadataChangeList::ModelTypeStateChange& state_change =
        changes->GetModelTypeStateChange();
    switch (state_change.type) {
      case SimpleMetadataChangeList::UPDATE:
        db_.set_model_type_state(state_change.state);
        break;
      case SimpleMetadataChangeList::CLEAR:
        db_.set_model_type_state(ModelTypeState());
        break;
    }
  }
}

void FakeModelTypeService::GetData(StorageKeyList keys, DataCallback callback) {
  if (service_error_.IsSet()) {
    callback.Run(service_error_, nullptr);
    service_error_ = SyncError();
    return;
  }
  std::unique_ptr<DataBatchImpl> batch(new DataBatchImpl());
  for (const std::string& key : keys) {
    DCHECK(db_.HasData(key)) << "No data for " << key;
    batch->Put(key, CopyEntityData(db_.GetData(key)));
  }
  callback.Run(SyncError(), std::move(batch));
}

void FakeModelTypeService::GetAllData(DataCallback callback) {
  if (service_error_.IsSet()) {
    callback.Run(service_error_, nullptr);
    service_error_ = SyncError();
    return;
  }
  std::unique_ptr<DataBatchImpl> batch(new DataBatchImpl());
  for (const auto& kv : db_.all_data()) {
    batch->Put(kv.first, CopyEntityData(*kv.second));
  }
  callback.Run(SyncError(), std::move(batch));
}

std::string FakeModelTypeService::GetClientTag(const EntityData& entity_data) {
  return ClientTagFromKey(entity_data.specifics.preference().name());
}

std::string FakeModelTypeService::GetStorageKey(const EntityData& entity_data) {
  return entity_data.specifics.preference().name();
}

void FakeModelTypeService::OnChangeProcessorSet() {}

void FakeModelTypeService::SetServiceError(SyncError::ErrorType error_type) {
  DCHECK(!service_error_.IsSet());
  service_error_ = SyncError(FROM_HERE, error_type, "TestError", PREFERENCES);
}

ConflictResolution FakeModelTypeService::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) const {
  DCHECK(conflict_resolution_);
  return std::move(*conflict_resolution_);
}

void FakeModelTypeService::SetConflictResolution(
    ConflictResolution resolution) {
  conflict_resolution_.reset(new ConflictResolution(std::move(resolution)));
}

void FakeModelTypeService::CheckPostConditions() {
  DCHECK(!service_error_.IsSet());
}

}  // namespace syncer
