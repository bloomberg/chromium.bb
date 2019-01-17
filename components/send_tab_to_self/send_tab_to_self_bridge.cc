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
#include "components/sync/device_info/device_info.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace send_tab_to_self {

SendTabToSelfBridge::SendTabToSelfBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::LocalDeviceInfoProvider* local_device_info_provider,
    base::Clock* clock)
    : ModelTypeSyncBridge(std::move(change_processor)),
      local_device_info_provider_(local_device_info_provider),
      clock_(clock) {
  DCHECK(local_device_info_provider_);
  DCHECK(clock_);
  auto batch = std::make_unique<syncer::MetadataBatch>();
  this->change_processor()->ModelReadyToSync(std::move(batch));
  // TODO(jeffreycohen): Create a local store and read meta data from it.
}

SendTabToSelfBridge::~SendTabToSelfBridge() {
}

std::unique_ptr<syncer::MetadataChangeList>
SendTabToSelfBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

base::Optional<syncer::ModelError> SendTabToSelfBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

base::Optional<syncer::ModelError> SendTabToSelfBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  for (syncer::EntityChange& change : entity_changes) {
    if (change.type() == syncer::EntityChange::ACTION_DELETE) {
      entries_.erase(change.storage_key());
    } else {
      const sync_pb::SendTabToSelfSpecifics& specifics =
          change.data().specifics.send_tab_to_self();
      // TODO(jeffreycohen): FromProto expects a valid entry. External data
      // needs to be sanitized before it's passed through.
      std::unique_ptr<SendTabToSelfEntry> entry =
          SendTabToSelfEntry::FromProto(specifics, clock_->Now());
      // This entry is new. Add it to the model.
      entries_[entry->GetGUID()] = std::move(entry);
    }
  }
  if (!entity_changes.empty()) {
    NotifySendTabToSelfModelChanged();
  }
  return base::nullopt;
}

void SendTabToSelfBridge::GetData(StorageKeyList storage_keys,
                                  DataCallback callback) {
  syncer::InMemoryMetadataChangeList metadata_change_list;

  for (const std::string& guid : storage_keys) {
    const SendTabToSelfEntry* entry = GetEntryByGUID(guid);
    if (!entry) {
      continue;
    }
    std::unique_ptr<sync_pb::SendTabToSelfSpecifics> specifics =
        entry->AsProto();
    auto entity_data = std::make_unique<syncer::EntityData>();
    *entity_data->specifics.mutable_send_tab_to_self() = *specifics;
    entity_data->non_unique_name = specifics->url();

    change_processor()->Put(specifics->guid(), std::move(entity_data),
                            &metadata_change_list);
  }
}

void SendTabToSelfBridge::GetAllDataForDebugging(DataCallback callback) {
  // TODO(jeffreycohen): Add once we have a batch model.
  NOTIMPLEMENTED();
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
  syncer::InMemoryMetadataChangeList metadata_change_list;

  for (const auto& key : GetAllGuids()) {
    change_processor()->Delete(key, &metadata_change_list);
  }
  entries_.clear();
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
    const std::string& title,
    base::Time navigation_time) {
  if (!change_processor()->IsTrackingMetadata()) {
    return nullptr;
  }

  std::string guid = base::GenerateGUID();

  // TODO(jeffreycohen) Handle the fact that local device info may not be
  // guaranteed in production code as it may take some time to get initialized.
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());

  // Assure that we don't have a guid collision.
  DCHECK_EQ(GetEntryByGUID(guid), nullptr);
  std::string trimmed_title = base::CollapseWhitespaceASCII(title, false);
  std::string device_name =
      local_device_info_provider_->GetLocalDeviceInfo()->client_name();

  auto entry = std::make_unique<SendTabToSelfEntry>(
      guid, url, trimmed_title, clock_->Now(), navigation_time, device_name);

  syncer::InMemoryMetadataChangeList metadata_change_list;

  // This entry is new. Add it to the store and model.
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_send_tab_to_self() = *entry->AsProto();
  entity_data->non_unique_name = entry->GetURL().spec();
  entity_data->creation_time = entry->GetSharedTime();

  change_processor()->Put(guid, std::move(entity_data), &metadata_change_list);

  return entries_.emplace(guid, std::move(entry)).first->second.get();
}

void SendTabToSelfBridge::NotifySendTabToSelfModelChanged() {
  for (SendTabToSelfModelObserver& observer : observers_)
    observer.SendTabToSelfModelChanged();
}

}  // namespace send_tab_to_self
