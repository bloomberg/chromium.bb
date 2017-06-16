// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/fake_model_type_sync_bridge.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::EntitySpecifics;
using sync_pb::EntityMetadata;
using sync_pb::ModelTypeState;

namespace syncer {

namespace {

// A simple InMemoryMetadataChangeList that provides accessors for its data.
class TestMetadataChangeList : public InMemoryMetadataChangeList {
 public:
  TestMetadataChangeList() {}
  ~TestMetadataChangeList() override {}

  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override {
    DCHECK(!storage_key.empty());
    InMemoryMetadataChangeList::UpdateMetadata(storage_key, metadata);
  }

  void ClearMetadata(const std::string& storage_key) override {
    DCHECK(!storage_key.empty());
    InMemoryMetadataChangeList::ClearMetadata(storage_key);
  }

  const std::map<std::string, MetadataChange>& GetMetadataChanges() const {
    return metadata_changes_;
  }

  bool HasModelTypeStateChange() const {
    return state_change_.get() != nullptr;
  }

  const ModelTypeStateChange& GetModelTypeStateChange() const {
    return *state_change_.get();
  }
};

}  // namespace

// static
std::string FakeModelTypeSyncBridge::ClientTagFromKey(const std::string& key) {
  return "ClientTag_" + key;
}

// static
std::string FakeModelTypeSyncBridge::TagHashFromKey(const std::string& key) {
  return GenerateSyncableHash(PREFERENCES,
                              FakeModelTypeSyncBridge::ClientTagFromKey(key));
}

// static
EntitySpecifics FakeModelTypeSyncBridge::GenerateSpecifics(
    const std::string& key,
    const std::string& value) {
  EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(key);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// static
std::unique_ptr<EntityData> FakeModelTypeSyncBridge::GenerateEntityData(
    const std::string& key,
    const std::string& value) {
  std::unique_ptr<EntityData> entity_data = base::MakeUnique<EntityData>();
  entity_data->client_tag_hash = TagHashFromKey(key);
  entity_data->specifics = GenerateSpecifics(key, value);
  entity_data->non_unique_name = key;
  return entity_data;
}

FakeModelTypeSyncBridge::Store::Store() {}
FakeModelTypeSyncBridge::Store::~Store() {}

void FakeModelTypeSyncBridge::Store::PutData(const std::string& key,
                                             const EntityData& data) {
  data_change_count_++;
  data_store_[key] = CopyEntityData(data);
}

void FakeModelTypeSyncBridge::Store::PutMetadata(
    const std::string& key,
    const EntityMetadata& metadata) {
  metadata_change_count_++;
  metadata_store_[key] = metadata;
}

void FakeModelTypeSyncBridge::Store::RemoveData(const std::string& key) {
  data_change_count_++;
  data_store_.erase(key);
}

void FakeModelTypeSyncBridge::Store::RemoveMetadata(const std::string& key) {
  metadata_change_count_++;
  metadata_store_.erase(key);
}

bool FakeModelTypeSyncBridge::Store::HasData(const std::string& key) const {
  return data_store_.find(key) != data_store_.end();
}

bool FakeModelTypeSyncBridge::Store::HasMetadata(const std::string& key) const {
  return metadata_store_.find(key) != metadata_store_.end();
}

const EntityData& FakeModelTypeSyncBridge::Store::GetData(
    const std::string& key) const {
  return *data_store_.find(key)->second;
}

const std::string& FakeModelTypeSyncBridge::Store::GetValue(
    const std::string& key) const {
  return GetData(key).specifics.preference().value();
}

const sync_pb::EntityMetadata& FakeModelTypeSyncBridge::Store::GetMetadata(
    const std::string& key) const {
  return metadata_store_.find(key)->second;
}

std::unique_ptr<MetadataBatch>
FakeModelTypeSyncBridge::Store::CreateMetadataBatch() const {
  std::unique_ptr<MetadataBatch> metadata_batch(new MetadataBatch());
  metadata_batch->SetModelTypeState(model_type_state_);
  for (const auto& kv : metadata_store_) {
    metadata_batch->AddMetadata(kv.first, kv.second);
  }
  return metadata_batch;
}

void FakeModelTypeSyncBridge::Store::Reset() {
  data_change_count_ = 0;
  metadata_change_count_ = 0;
  data_store_.clear();
  metadata_store_.clear();
  model_type_state_.Clear();
}

FakeModelTypeSyncBridge::FakeModelTypeSyncBridge(
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeSyncBridge(change_processor_factory, PREFERENCES),
      db_(base::MakeUnique<Store>()) {}

FakeModelTypeSyncBridge::~FakeModelTypeSyncBridge() {
  EXPECT_FALSE(error_next_);
}

EntitySpecifics FakeModelTypeSyncBridge::WriteItem(const std::string& key,
                                                   const std::string& value) {
  std::unique_ptr<EntityData> entity_data = GenerateEntityData(key, value);
  EntitySpecifics specifics_copy = entity_data->specifics;
  WriteItem(key, std::move(entity_data));
  return specifics_copy;
}

// Overloaded form to allow passing of custom entity data.
void FakeModelTypeSyncBridge::WriteItem(
    const std::string& key,
    std::unique_ptr<EntityData> entity_data) {
  db_->PutData(key, *entity_data);
  if (change_processor()->IsTrackingMetadata()) {
    auto change_list = CreateMetadataChangeList();
    change_processor()->Put(key, std::move(entity_data), change_list.get());
    ApplyMetadataChangeList(std::move(change_list));
  }
}

void FakeModelTypeSyncBridge::DeleteItem(const std::string& key) {
  db_->RemoveData(key);
  if (change_processor()->IsTrackingMetadata()) {
    auto change_list = CreateMetadataChangeList();
    change_processor()->Delete(key, change_list.get());
    ApplyMetadataChangeList(std::move(change_list));
  }
}

std::unique_ptr<MetadataChangeList>
FakeModelTypeSyncBridge::CreateMetadataChangeList() {
  return base::MakeUnique<TestMetadataChangeList>();
}

base::Optional<ModelError> FakeModelTypeSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  if (error_next_) {
    error_next_ = false;
    return ModelError(FROM_HERE, "boom");
  }

  std::set<std::string> remote_storage_keys;
  // Store any new remote entities.
  for (const auto& change : entity_data) {
    EXPECT_FALSE(change.data().is_deleted());
    EXPECT_EQ(EntityChange::ACTION_ADD, change.type());
    std::string storage_key = change.storage_key();
    EXPECT_NE(SupportsGetStorageKey(), storage_key.empty());
    if (storage_key.empty()) {
      storage_key = GetStorageKeyImpl(change.data());
      if (base::ContainsKey(keys_to_ignore_, storage_key)) {
        change_processor()->UntrackEntity(change.data());
      } else {
        change_processor()->UpdateStorageKey(change.data(), storage_key,
                                             metadata_change_list.get());
      }
    }
    remote_storage_keys.insert(storage_key);
    db_->PutData(storage_key, change.data());
  }

  // Commit any local entities that aren't being overwritten by the server.
  for (const auto& kv : db_->all_data()) {
    if (remote_storage_keys.find(kv.first) == remote_storage_keys.end()) {
      change_processor()->Put(kv.first, CopyEntityData(*kv.second),
                              metadata_change_list.get());
    }
  }

  ApplyMetadataChangeList(std::move(metadata_change_list));
  return {};
}

base::Optional<ModelError> FakeModelTypeSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_changes,
    EntityChangeList entity_changes) {
  if (error_next_) {
    error_next_ = false;
    return ModelError(FROM_HERE, "boom");
  }

  for (const EntityChange& change : entity_changes) {
    switch (change.type()) {
      case EntityChange::ACTION_ADD: {
        std::string storage_key = change.storage_key();
        EXPECT_NE(SupportsGetStorageKey(), storage_key.empty());
        if (storage_key.empty()) {
          storage_key = GetStorageKeyImpl(change.data());
          change_processor()->UpdateStorageKey(change.data(), storage_key,
                                               metadata_changes.get());
        }
        EXPECT_FALSE(db_->HasData(storage_key));
        db_->PutData(storage_key, change.data());
        break;
      }
      case EntityChange::ACTION_UPDATE:
        EXPECT_TRUE(db_->HasData(change.storage_key()));
        db_->PutData(change.storage_key(), change.data());
        break;
      case EntityChange::ACTION_DELETE:
        EXPECT_TRUE(db_->HasData(change.storage_key()));
        db_->RemoveData(change.storage_key());
        break;
    }
  }
  ApplyMetadataChangeList(std::move(metadata_changes));
  return {};
}

void FakeModelTypeSyncBridge::ApplyMetadataChangeList(
    std::unique_ptr<MetadataChangeList> mcl) {
  DCHECK(mcl);
  TestMetadataChangeList* tmcl =
      static_cast<TestMetadataChangeList*>(mcl.get());
  const auto& metadata_changes = tmcl->GetMetadataChanges();
  for (const auto& kv : metadata_changes) {
    switch (kv.second.type) {
      case TestMetadataChangeList::UPDATE:
        db_->PutMetadata(kv.first, kv.second.metadata);
        break;
      case TestMetadataChangeList::CLEAR:
        db_->RemoveMetadata(kv.first);
        break;
    }
  }
  if (tmcl->HasModelTypeStateChange()) {
    const auto& state_change = tmcl->GetModelTypeStateChange();
    switch (state_change.type) {
      case TestMetadataChangeList::UPDATE:
        db_->set_model_type_state(state_change.state);
        break;
      case TestMetadataChangeList::CLEAR:
        db_->set_model_type_state(ModelTypeState());
        break;
    }
  }
}

void FakeModelTypeSyncBridge::GetData(StorageKeyList keys,
                                      DataCallback callback) {
  if (error_next_) {
    error_next_ = false;
    change_processor()->ReportError(FROM_HERE, "boom");
    return;
  }

  auto batch = base::MakeUnique<MutableDataBatch>();
  for (const std::string& key : keys) {
    DCHECK(db_->HasData(key)) << "No data for " << key;
    batch->Put(key, CopyEntityData(db_->GetData(key)));
  }
  callback.Run(std::move(batch));
}

void FakeModelTypeSyncBridge::GetAllData(DataCallback callback) {
  if (error_next_) {
    error_next_ = false;
    change_processor()->ReportError(FROM_HERE, "boom");
    return;
  }

  auto batch = base::MakeUnique<MutableDataBatch>();
  for (const auto& kv : db_->all_data()) {
    batch->Put(kv.first, CopyEntityData(*kv.second));
  }
  callback.Run(std::move(batch));
}

std::string FakeModelTypeSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  return ClientTagFromKey(entity_data.specifics.preference().name());
}

std::string FakeModelTypeSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  DCHECK(supports_get_storage_key_);
  return GetStorageKeyImpl(entity_data);
}

std::string FakeModelTypeSyncBridge::GetStorageKeyImpl(
    const EntityData& entity_data) {
  return entity_data.specifics.preference().name();
}

bool FakeModelTypeSyncBridge::SupportsGetStorageKey() const {
  return supports_get_storage_key_;
}

void FakeModelTypeSyncBridge::SetSupportsGetStorageKey(
    bool supports_get_storage_key) {
  supports_get_storage_key_ = supports_get_storage_key;
}

ConflictResolution FakeModelTypeSyncBridge::ResolveConflict(
    const EntityData& local_data,
    const EntityData& remote_data) const {
  DCHECK(conflict_resolution_);
  return std::move(*conflict_resolution_);
}

void FakeModelTypeSyncBridge::SetConflictResolution(
    ConflictResolution resolution) {
  conflict_resolution_ =
      base::MakeUnique<ConflictResolution>(std::move(resolution));
}

void FakeModelTypeSyncBridge::ErrorOnNextCall() {
  EXPECT_FALSE(error_next_);
  error_next_ = true;
}

std::unique_ptr<EntityData> FakeModelTypeSyncBridge::CopyEntityData(
    const EntityData& old_data) {
  std::unique_ptr<EntityData> new_data(new EntityData());
  new_data->id = old_data.id;
  new_data->client_tag_hash = old_data.client_tag_hash;
  new_data->non_unique_name = old_data.non_unique_name;
  new_data->specifics = old_data.specifics;
  new_data->creation_time = old_data.creation_time;
  new_data->modification_time = old_data.modification_time;
  return new_data;
}

void FakeModelTypeSyncBridge::SetKeyToIgnore(const std::string key) {
  keys_to_ignore_.insert(key);
}

}  // namespace syncer
