// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_bridge.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace send_tab_to_self {

namespace {

using syncer::ModelTypeStore;

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(proto_t));
}

// Allocate a EntityData and copies |specifics| into it.
std::unique_ptr<syncer::EntityData> CopyToEntityData(
    const sync_pb::SendTabToSelfSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_send_tab_to_self() = specifics;
  entity_data->non_unique_name = specifics.url();
  entity_data->creation_time = ProtoTimeToTime(specifics.shared_time_usec());
  return entity_data;
}

}  // namespace

SendTabToSelfBridge::SendTabToSelfBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    base::Clock* clock,
    syncer::OnceModelTypeStoreFactory create_store_callback)
    : ModelTypeSyncBridge(std::move(change_processor)),
      clock_(clock),
      local_device_info_provider_(local_device_info_provider),
      weak_ptr_factory_(this) {
  DCHECK(local_device_info_provider);
  DCHECK(clock_);

  std::move(create_store_callback)
      .Run(syncer::SEND_TAB_TO_SELF,
           base::BindOnce(&SendTabToSelfBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

SendTabToSelfBridge::~SendTabToSelfBridge() {
}

std::unique_ptr<syncer::MetadataChangeList>
SendTabToSelfBridge::CreateMetadataChangeList() {
  return ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

base::Optional<syncer::ModelError> SendTabToSelfBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(entries_.empty());
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

base::Optional<syncer::ModelError> SendTabToSelfBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::vector<const SendTabToSelfEntry*> added;
  std::vector<std::string> removed;
  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (syncer::EntityChange& change : entity_changes) {
    const std::string& guid = change.storage_key();
    if (change.type() == syncer::EntityChange::ACTION_DELETE) {
      if (entries_.find(guid) != entries_.end()) {
        entries_.erase(change.storage_key());
        batch->DeleteData(guid);
        removed.push_back(change.storage_key());
      }
    } else {
      // syncer::EntityChange::ACTION_UPDATE is not supported by this bridge
      DCHECK(change.type() == syncer::EntityChange::ACTION_ADD);

      const sync_pb::SendTabToSelfSpecifics& specifics =
          change.data().specifics.send_tab_to_self();

      std::unique_ptr<SendTabToSelfEntry> entry =
          SendTabToSelfEntry::FromProto(specifics, clock_->Now());
      // This entry is new. Add it to the model.
      added.push_back(entry.get());
      SendTabToSelfLocal entry_pb = entry->AsLocalProto();
      entries_[entry->GetGUID()] = std::move(entry);

      // Write to the store.
      batch->WriteData(guid, entry_pb.SerializeAsString());
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));

  if (!removed.empty()) {
    NotifyRemoteSendTabToSelfEntryDeleted(removed);
  }
  if (!added.empty()) {
    NotifyRemoteSendTabToSelfEntryAdded(added);
  }

  return base::nullopt;
}

void SendTabToSelfBridge::GetData(StorageKeyList storage_keys,
                                  DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& guid : storage_keys) {
    const SendTabToSelfEntry* entry = GetEntryByGUID(guid);
    if (!entry) {
      continue;
    }

    batch->Put(guid, CopyToEntityData(entry->AsLocalProto().specifics()));
  }
  std::move(callback).Run(std::move(batch));
}

void SendTabToSelfBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& it : entries_) {
    batch->Put(it.first,
               CopyToEntityData(it.second->AsLocalProto().specifics()));
  }
  std::move(callback).Run(std::move(batch));
}

std::string SendTabToSelfBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string SendTabToSelfBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.send_tab_to_self().guid();
}

std::vector<std::string> SendTabToSelfBridge::GetAllGuids() const {
  std::vector<std::string> keys;
  for (const auto& it : entries_) {
    DCHECK_EQ(it.first, it.second->GetGUID());
    keys.push_back(it.first);
  }
  return keys;
}

void SendTabToSelfBridge::DeleteAllEntries() {
  if (!change_processor()->IsTrackingMetadata()) {
    DCHECK_EQ(0ul, entries_.size());
    return;
  }

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  std::vector<std::string> all_guids = GetAllGuids();

  for (const auto& guid : all_guids) {
    change_processor()->Delete(guid, batch->GetMetadataChangeList());
    batch->DeleteData(guid);
  }
  entries_.clear();

  NotifyRemoteSendTabToSelfEntryDeleted(all_guids);
}

const SendTabToSelfEntry* SendTabToSelfBridge::GetEntryByGUID(
    const std::string& guid) const {
  auto it = entries_.find(guid);
  if (it == entries_.end()) {
    return nullptr;
  }
  return it->second.get();
}

const SendTabToSelfEntry* SendTabToSelfBridge::AddEntry(
    const GURL& url,
    const std::string& title) {
  if (!change_processor()->IsTrackingMetadata()) {
    return nullptr;
  }

  if (!url.is_valid()) {
    return nullptr;
  }

  std::string guid = base::GenerateGUID();

  // Assure that we don't have a guid collision.
  DCHECK_EQ(GetEntryByGUID(guid), nullptr);

  std::string trimmed_title = "";

  if (base::IsStringUTF8(title)) {
    trimmed_title = base::CollapseWhitespaceASCII(title, false);
  }

  // TODO(crbug.com/938102) Use history service to find most recent navigation
  // time for url.
  base::Time navigation_time = clock_->Now();

  auto entry = std::make_unique<SendTabToSelfEntry>(
      guid, url, trimmed_title, clock_->Now(), navigation_time,
      local_device_name_);

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  // This entry is new. Add it to the store and model.
  auto entity_data = CopyToEntityData(entry->AsLocalProto().specifics());

  change_processor()->Put(guid, std::move(entity_data),
                          batch->GetMetadataChangeList());

  const SendTabToSelfEntry* result =
      entries_.emplace(guid, std::move(entry)).first->second.get();
  SendTabToSelfLocal specifics = result->AsLocalProto();

  batch->WriteData(guid, specifics.SerializeAsString());

  Commit(std::move(batch));
  return result;
}

void SendTabToSelfBridge::DeleteEntry(const std::string& guid) {
  // Assure that an entry with that guid exists.
  if (GetEntryByGUID(guid) == nullptr) {
    return;
  }

  DCHECK(change_processor()->IsTrackingMetadata());

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  change_processor()->Delete(guid, batch->GetMetadataChangeList());

  entries_.erase(guid);
  batch->DeleteData(guid);
  Commit(std::move(batch));
}

void SendTabToSelfBridge::DismissEntry(const std::string& guid) {
  SendTabToSelfEntry* entry = GetMutableEntryByGUID(guid);
  // Assure that an entry with that guid exists.
  if (!entry) {
    return;
  }
  entry->SetNotificationDismissed(true);

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  batch->WriteData(guid, entry->AsLocalProto().SerializeAsString());
  Commit(std::move(batch));
}

// static
std::unique_ptr<syncer::ModelTypeStore>
SendTabToSelfBridge::DestroyAndStealStoreForTest(
    std::unique_ptr<SendTabToSelfBridge> bridge) {
  return std::move(bridge->store_);
}

void SendTabToSelfBridge::NotifyRemoteSendTabToSelfEntryAdded(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.EntriesAddedRemotely(new_entries);
  }
}

void SendTabToSelfBridge::NotifyRemoteSendTabToSelfEntryDeleted(
    const std::vector<std::string>& guids) {
  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.EntriesRemovedRemotely(guids);
  }
}

void SendTabToSelfBridge::NotifySendTabToSelfModelLoaded() {
  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.SendTabToSelfModelLoaded();
  }
}

void SendTabToSelfBridge::OnStoreCreated(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&SendTabToSelfBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void SendTabToSelfBridge::OnReadAllData(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::ModelTypeStore::Record& r : *record_list) {
    auto specifics = std::make_unique<SendTabToSelfLocal>();
    if (specifics->ParseFromString(r.value)) {
      entries_[specifics->specifics().guid()] =
          SendTabToSelfEntry::FromLocalProto(*specifics, clock_->Now());
    } else {
      change_processor()->ReportError(
          {FROM_HERE, "Failed to deserialize specifics."});
      return;
    }
  }

  if (local_device_info_provider_->GetLocalDeviceInfo()) {
    OnDeviceProviderInitialized();
  } else {
    device_subscription_ =
        local_device_info_provider_->RegisterOnInitializedCallback(
            base::BindRepeating(
                &SendTabToSelfBridge::OnDeviceProviderInitialized,
                base::Unretained(this)));
  }
}

void SendTabToSelfBridge::OnDeviceProviderInitialized() {
  device_subscription_.reset();
  local_device_name_ =
      local_device_info_provider_->GetLocalDeviceInfo()->client_name();

  // now that all of the providers are ready we can read the metadata.
  store_->ReadAllMetadata(base::BindOnce(
      &SendTabToSelfBridge::OnReadAllMetadata, weak_ptr_factory_.GetWeakPtr()));
}

void SendTabToSelfBridge::OnReadAllMetadata(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  NotifySendTabToSelfModelLoaded();
}

void SendTabToSelfBridge::OnCommit(
    const base::Optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void SendTabToSelfBridge::Commit(
    std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&SendTabToSelfBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

SendTabToSelfEntry* SendTabToSelfBridge::GetMutableEntryByGUID(
    const std::string& guid) const {
  auto it = entries_.find(guid);
  if (it == entries_.end()) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace send_tab_to_self
