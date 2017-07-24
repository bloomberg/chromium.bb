// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/user_events/user_event_sync_bridge.h"

#include <set>
#include <utility>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using sync_pb::UserEventSpecifics;
using sync_pb::ModelTypeState;
using IdList = ModelTypeStore::IdList;
using Record = ModelTypeStore::Record;
using RecordList = ModelTypeStore::RecordList;
using Result = ModelTypeStore::Result;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

std::string GetStorageKeyFromSpecifics(const UserEventSpecifics& specifics) {
  // Force Big Endian, this means newly created keys are last in sort order,
  // which allows leveldb to append new writes, which it is best at.
  // TODO(skym): Until we force |event_time_usec| to never conflict, this has
  // the potential for errors.
  std::string key(8, 0);
  base::WriteBigEndian(&key[0], specifics.event_time_usec());
  return key;
}

int64_t GetEventTimeFromStorageKey(const std::string& storage_key) {
  int64_t event_time;
  base::ReadBigEndian(&storage_key[0], &event_time);
  return event_time;
}

std::unique_ptr<EntityData> MoveToEntityData(
    std::unique_ptr<UserEventSpecifics> specifics) {
  auto entity_data = base::MakeUnique<EntityData>();
  entity_data->non_unique_name =
      base::Int64ToString(specifics->event_time_usec());
  entity_data->specifics.set_allocated_user_event(specifics.release());
  return entity_data;
}

std::unique_ptr<EntityData> CopyToEntityData(
    const UserEventSpecifics specifics) {
  auto entity_data = base::MakeUnique<EntityData>();
  entity_data->non_unique_name =
      base::Int64ToString(specifics.event_time_usec());
  *entity_data->specifics.mutable_user_event() = specifics;
  return entity_data;
}

}  // namespace

UserEventSyncBridge::UserEventSyncBridge(
    const ModelTypeStoreFactory& store_factory,
    const ChangeProcessorFactory& change_processor_factory,
    GlobalIdMapper* global_id_mapper)
    : ModelTypeSyncBridge(change_processor_factory, USER_EVENTS),
      global_id_mapper_(global_id_mapper) {
  store_factory.Run(
      base::Bind(&UserEventSyncBridge::OnStoreCreated, base::AsWeakPtr(this)));
  global_id_mapper_->AddGlobalIdChangeObserver(base::Bind(
      &UserEventSyncBridge::HandleGlobalIdChange, base::AsWeakPtr(this)));
}

UserEventSyncBridge::~UserEventSyncBridge() {}

std::unique_ptr<MetadataChangeList>
UserEventSyncBridge::CreateMetadataChangeList() {
  return WriteBatch::CreateMetadataChangeList();
}

base::Optional<ModelError> UserEventSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  NOTREACHED();
  return {};
}

base::Optional<ModelError> UserEventSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  std::set<int64_t> deleted_event_times;
  for (EntityChange& change : entity_changes) {
    DCHECK_EQ(EntityChange::ACTION_DELETE, change.type());
    batch->DeleteData(change.storage_key());
    deleted_event_times.insert(
        GetEventTimeFromStorageKey(change.storage_key()));
  }

  // Because we receive ApplySyncChanges with deletions when our commits are
  // confirmed, this is the perfect time to cleanup our in flight objects which
  // are no longer in flight.
  base::EraseIf(in_flight_nav_linked_events_,
                [&deleted_event_times](
                    const std::pair<int64_t, sync_pb::UserEventSpecifics> kv) {
                  return base::ContainsKey(deleted_event_times,
                                           kv.second.event_time_usec());
                });

  batch->TransferMetadataChanges(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch),
      base::Bind(&UserEventSyncBridge::OnCommit, base::AsWeakPtr(this)));
  return {};
}

void UserEventSyncBridge::GetData(StorageKeyList storage_keys,
                                  DataCallback callback) {
  store_->ReadData(storage_keys, base::Bind(&UserEventSyncBridge::OnReadData,
                                            base::AsWeakPtr(this), callback));
}

void UserEventSyncBridge::GetAllData(DataCallback callback) {
  store_->ReadAllData(base::Bind(&UserEventSyncBridge::OnReadAllData,
                                 base::AsWeakPtr(this), callback));
}

std::string UserEventSyncBridge::GetClientTag(const EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string UserEventSyncBridge::GetStorageKey(const EntityData& entity_data) {
  return GetStorageKeyFromSpecifics(entity_data.specifics.user_event());
}

void UserEventSyncBridge::DisableSync() {
  // No data should be retained through sign out.
  store_->ReadAllData(base::Bind(&UserEventSyncBridge::OnReadAllDataToDelete,
                                 base::AsWeakPtr(this)));
}

void UserEventSyncBridge::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  std::string storage_key = GetStorageKeyFromSpecifics(*specifics);

  // There are two scenarios we need to guard against here. First, the given
  // user even may have been read from an old global_id timestamp off of a
  // navigation, which has already been re-written. In this case, we should be
  // able to look up the latest/best global_id to use right now, and update as
  // such. The other scenario is that the navigation is going to be updated in
  // the future, and the current global_id, while valid for now, is never going
  // to make it to the server, and will need to be fixed. To handle this
  // scenario, we store a specifics copy in |in in_flight_nav_linked_events_|,
  // and will re-record in HandleGlobalIdChange.
  if (specifics->has_navigation_id()) {
    int64_t latest_global_id =
        global_id_mapper_->GetLatestGlobalId(specifics->navigation_id());
    specifics->set_navigation_id(latest_global_id);
    in_flight_nav_linked_events_.insert(
        std::make_pair(latest_global_id, *specifics));
  }

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  batch->WriteData(storage_key, specifics->SerializeAsString());

  change_processor()->Put(storage_key, MoveToEntityData(std::move(specifics)),
                          batch->GetMetadataChangeList());
  store_->CommitWriteBatch(
      std::move(batch),
      base::Bind(&UserEventSyncBridge::OnCommit, base::AsWeakPtr(this)));
}

void UserEventSyncBridge::OnStoreCreated(
    Result result,
    std::unique_ptr<ModelTypeStore> store) {
  if (result == Result::SUCCESS) {
    std::swap(store_, store);
    store_->ReadAllMetadata(base::Bind(&UserEventSyncBridge::OnReadAllMetadata,
                                       base::AsWeakPtr(this)));
  } else {
    change_processor()->ReportError(FROM_HERE,
                                    "ModelTypeStore creation failed.");
  }
}

void UserEventSyncBridge::OnReadAllMetadata(
    base::Optional<ModelError> error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(error.value());
  } else {
    if (!metadata_batch->GetModelTypeState().initial_sync_done()) {
      // We have never initialized before, force it to true. We are not going to
      // ever have a GetUpdates because our type is commit only.
      // TODO(skym): Do we need to worry about saving this back ourselves? Or
      // does that get taken care for us?
      ModelTypeState state = metadata_batch->GetModelTypeState();
      state.set_initial_sync_done(true);
      metadata_batch->SetModelTypeState(state);
    }
    change_processor()->ModelReadyToSync(std::move(metadata_batch));
  }
}

void UserEventSyncBridge::OnCommit(Result result) {
  if (result != Result::SUCCESS) {
    change_processor()->ReportError(FROM_HERE, "Failed writing user events.");
  }
}

void UserEventSyncBridge::OnReadData(DataCallback callback,
                                     Result result,
                                     std::unique_ptr<RecordList> data_records,
                                     std::unique_ptr<IdList> missing_id_list) {
  OnReadAllData(callback, result, std::move(data_records));
}

void UserEventSyncBridge::OnReadAllData(
    DataCallback callback,
    Result result,
    std::unique_ptr<RecordList> data_records) {
  if (result != Result::SUCCESS) {
    change_processor()->ReportError(FROM_HERE, "Failed reading user events.");
    return;
  }

  auto batch = base::MakeUnique<MutableDataBatch>();
  UserEventSpecifics specifics;
  for (const Record& r : *data_records) {
    if (specifics.ParseFromString(r.value)) {
      DCHECK_EQ(r.id, GetStorageKeyFromSpecifics(specifics));
      batch->Put(r.id, CopyToEntityData(specifics));
    } else {
      change_processor()->ReportError(FROM_HERE,
                                      "Failed deserializing user events.");
      return;
    }
  }
  callback.Run(std::move(batch));
}

void UserEventSyncBridge::OnReadAllDataToDelete(
    Result result,
    std::unique_ptr<RecordList> data_records) {
  if (result != Result::SUCCESS) {
    change_processor()->ReportError(FROM_HERE, "Failed reading user events.");
    return;
  }

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const Record& r : *data_records) {
    batch->DeleteData(r.id);
  }
  store_->CommitWriteBatch(
      std::move(batch),
      base::Bind(&UserEventSyncBridge::OnCommit, base::AsWeakPtr(this)));
}

void UserEventSyncBridge::HandleGlobalIdChange(int64_t old_global_id,
                                               int64_t new_global_id) {
  DCHECK_NE(old_global_id, new_global_id);
  auto iter = in_flight_nav_linked_events_.find(old_global_id);
  while (iter != in_flight_nav_linked_events_.end()) {
    auto specifics = base::MakeUnique<UserEventSpecifics>(iter->second);
    DCHECK_EQ(old_global_id, specifics->navigation_id());

    iter = in_flight_nav_linked_events_.erase(iter);

    specifics->set_navigation_id(new_global_id);
    RecordUserEvent(std::move(specifics));
  }
}

}  // namespace syncer
