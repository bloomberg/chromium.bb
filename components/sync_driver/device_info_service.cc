// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_service.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "sync/api/entity_change.h"
#include "sync/api/metadata_batch.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/data_batch_impl.h"
#include "sync/internal_api/public/simple_metadata_change_list.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/protocol/sync.pb.h"

namespace sync_driver_v2 {

using syncer::SyncError;
using syncer_v2::DataBatchImpl;
using syncer_v2::EntityChange;
using syncer_v2::EntityChangeList;
using syncer_v2::EntityData;
using syncer_v2::EntityDataMap;
using syncer_v2::MetadataBatch;
using syncer_v2::MetadataChangeList;
using syncer_v2::ModelTypeStore;
using syncer_v2::SimpleMetadataChangeList;
using sync_driver::DeviceInfo;
using sync_pb::DataTypeState;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;

using Record = ModelTypeStore::Record;
using RecordList = ModelTypeStore::RecordList;
using Result = ModelTypeStore::Result;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

const char kClientTagPrefix[] = "DeviceInfo_";

}  // namespace

DeviceInfoService::DeviceInfoService(
    sync_driver::LocalDeviceInfoProvider* local_device_info_provider,
    const StoreFactoryFunction& callback,
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeService(change_processor_factory, syncer::DEVICE_INFO),
      local_device_info_provider_(local_device_info_provider),
      weak_factory_(this) {
  DCHECK(local_device_info_provider);

  // This is not threadsafe, but presuably the provider initializes on the same
  // thread as us so we're okay.
  if (local_device_info_provider->GetLocalDeviceInfo()) {
    OnProviderInitialized();
  } else {
    subscription_ =
        local_device_info_provider->RegisterOnInitializedCallback(base::Bind(
            &DeviceInfoService::OnProviderInitialized, base::Unretained(this)));
  }

  callback.Run(base::Bind(&DeviceInfoService::OnStoreCreated,
                          weak_factory_.GetWeakPtr()));
}

DeviceInfoService::~DeviceInfoService() {}

std::unique_ptr<MetadataChangeList>
DeviceInfoService::CreateMetadataChangeList() {
  return base::WrapUnique(new SimpleMetadataChangeList());
}

SyncError DeviceInfoService::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityDataMap entity_data_map) {
  if (!has_provider_initialized_ || !has_metadata_loaded_ ||
      !change_processor()) {
    return SyncError(
        FROM_HERE, SyncError::DATATYPE_ERROR,
        "Cannot call MergeSyncData without provider, data, and processor.",
        syncer::DEVICE_INFO);
  }

  // Local data should typically be near empty, with the only possible value
  // corresponding to this device. This is because on signout all device info
  // data is blown away. However, this simplification is being ignored here and
  // a full difference is going to be calculated to explore what other service
  // implementations may look like.
  std::set<std::string> local_guids_to_put;
  for (const auto& kv : all_data_) {
    local_guids_to_put.insert(kv.first);
  }

  bool has_changes = false;
  const DeviceInfo* local_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  std::string local_guid = local_info->guid();
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const auto& kv : entity_data_map) {
    const DeviceInfoSpecifics& specifics =
        kv.second.value().specifics.device_info();
    DCHECK_EQ(kv.first, SpecificsToTag(specifics));
    if (specifics.cache_guid() == local_guid) {
      // Don't Put local data if it's the same as the remote copy.
      if (local_info->Equals(*CopyToModel(specifics))) {
        local_guids_to_put.erase(local_guid);
      }
    } else {
      // Remote data wins conflicts.
      local_guids_to_put.erase(specifics.cache_guid());
      has_changes = true;
      StoreSpecifics(base::WrapUnique(new DeviceInfoSpecifics(specifics)),
                     batch.get());
    }
  }

  for (const std::string& guid : local_guids_to_put) {
    change_processor()->Put(SpecificsToTag(*all_data_[guid]),
                            CopyToEntityData(*all_data_[guid]),
                            metadata_change_list.get());
  }

  CommitAndNotify(std::move(batch), std::move(metadata_change_list),
                  has_changes);
  return syncer::SyncError();
}

SyncError DeviceInfoService::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  if (!has_provider_initialized_ || !has_metadata_loaded_) {
    return SyncError(
        FROM_HERE, SyncError::DATATYPE_ERROR,
        "Cannot call ApplySyncChanges before provider and data have loaded.",
        syncer::DEVICE_INFO);
  }

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  bool has_changes = false;
  for (EntityChange& change : entity_changes) {
    const std::string guid = TagToCacheGuid(change.client_tag());
    // Each device is the authoritative source for itself, ignore any remote
    // changes that have our local cache guid.
    if (guid == local_device_info_provider_->GetLocalDeviceInfo()->guid()) {
      continue;
    }

    if (change.type() == EntityChange::ACTION_DELETE) {
      has_changes |= DeleteSpecifics(guid, batch.get());
    } else {
      const DeviceInfoSpecifics& specifics =
          change.data().specifics.device_info();
      DCHECK(guid == specifics.cache_guid());
      StoreSpecifics(base::WrapUnique(new DeviceInfoSpecifics(specifics)),
                     batch.get());
      has_changes = true;
    }
  }

  CommitAndNotify(std::move(batch), std::move(metadata_change_list),
                  has_changes);
  return SyncError();
}

void DeviceInfoService::GetData(ClientTagList client_tags,
                                DataCallback callback) {
  if (!has_metadata_loaded_) {
    callback.Run(
        SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                  "Should not call GetData before metadata has loaded.",
                  syncer::DEVICE_INFO),
        std::unique_ptr<DataBatchImpl>());
    return;
  }

  std::unique_ptr<DataBatchImpl> batch(new DataBatchImpl());
  for (const auto& tag : client_tags) {
    const auto& iter = all_data_.find(TagToCacheGuid(tag));
    if (iter != all_data_.end()) {
      DCHECK_EQ(tag, SpecificsToTag(*iter->second));
      batch->Put(tag, CopyToEntityData(*iter->second));
    }
  }
  callback.Run(syncer::SyncError(), std::move(batch));
}

void DeviceInfoService::GetAllData(DataCallback callback) {
  if (!has_metadata_loaded_) {
    callback.Run(
        SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                  "Should not call GetAllData before metadata has loaded.",
                  syncer::DEVICE_INFO),
        std::unique_ptr<DataBatchImpl>());
    return;
  }

  std::unique_ptr<DataBatchImpl> batch(new DataBatchImpl());
  for (const auto& kv : all_data_) {
    batch->Put(SpecificsToTag(*kv.second), CopyToEntityData(*kv.second));
  }
  callback.Run(syncer::SyncError(), std::move(batch));
}

std::string DeviceInfoService::GetClientTag(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return SpecificsToTag(entity_data.specifics.device_info());
}

void DeviceInfoService::OnChangeProcessorSet() {
  // We've recieved a new processor that needs metadata. If we're still in the
  // process of loading data and/or metadata, then |has_metadata_loaded_| is
  // false and we'll wait for those async reads to happen. If we've already
  // loaded metadata and then subsequently we get a new processor, we must not
  // have created the processor ourselves because we had no metadata. So there
  // must not be any metadata on disk.
  if (has_metadata_loaded_) {
    change_processor()->OnMetadataLoaded(base::WrapUnique(new MetadataBatch()));
    TryReconcileLocalAndStored();
  }
}

bool DeviceInfoService::IsSyncing() const {
  return !all_data_.empty();
}

std::unique_ptr<DeviceInfo> DeviceInfoService::GetDeviceInfo(
    const std::string& client_id) const {
  const ClientIdToSpecifics::const_iterator iter = all_data_.find(client_id);
  if (iter == all_data_.end()) {
    return std::unique_ptr<DeviceInfo>();
  }

  return CopyToModel(*iter->second);
}

ScopedVector<DeviceInfo> DeviceInfoService::GetAllDeviceInfo() const {
  ScopedVector<DeviceInfo> list;

  for (ClientIdToSpecifics::const_iterator iter = all_data_.begin();
       iter != all_data_.end(); ++iter) {
    list.push_back(CopyToModel(*iter->second));
  }

  return list;
}

void DeviceInfoService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceInfoService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

int DeviceInfoService::CountActiveDevices() const {
  // TODO(skym): crbug.com/590006: Implementation.
  return 0;
}

void DeviceInfoService::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceInfoChange());
}

std::string DeviceInfoService::SpecificsToTag(
    const sync_pb::DeviceInfoSpecifics& specifics) {
  return kClientTagPrefix + specifics.cache_guid();
}

std::string DeviceInfoService::TagToCacheGuid(const std::string& tag) {
  DCHECK(base::StartsWith(tag, kClientTagPrefix, base::CompareCase::SENSITIVE));
  return tag.substr(strlen(kClientTagPrefix));
}

// TODO(skym): crbug.com/543406: It might not make sense for this to be a
// scoped_ptr.
// Static.
std::unique_ptr<DeviceInfoSpecifics> DeviceInfoService::CopyToSpecifics(
    const DeviceInfo& info) {
  std::unique_ptr<DeviceInfoSpecifics> specifics =
      base::WrapUnique(new DeviceInfoSpecifics);
  specifics->set_cache_guid(info.guid());
  specifics->set_client_name(info.client_name());
  specifics->set_chrome_version(info.chrome_version());
  specifics->set_sync_user_agent(info.sync_user_agent());
  specifics->set_device_type(info.device_type());
  specifics->set_signin_scoped_device_id(info.signin_scoped_device_id());
  return specifics;
}

// Static.
std::unique_ptr<DeviceInfo> DeviceInfoService::CopyToModel(
    const DeviceInfoSpecifics& specifics) {
  return base::WrapUnique(new DeviceInfo(
      specifics.cache_guid(), specifics.client_name(),
      specifics.chrome_version(), specifics.sync_user_agent(),
      specifics.device_type(), specifics.signin_scoped_device_id()));
}

// Static.
std::unique_ptr<EntityData> DeviceInfoService::CopyToEntityData(
    const DeviceInfoSpecifics& specifics) {
  std::unique_ptr<EntityData> entity_data(new EntityData());
  *entity_data->specifics.mutable_device_info() = specifics;
  entity_data->non_unique_name = specifics.client_name();
  return entity_data;
}

void DeviceInfoService::StoreSpecifics(
    std::unique_ptr<DeviceInfoSpecifics> specifics,
    WriteBatch* batch) {
  const std::string guid = specifics->cache_guid();
  DVLOG(1) << "Storing DEVICE_INFO for " << specifics->client_name()
           << " with ID " << guid;
  store_->WriteData(batch, guid, specifics->SerializeAsString());
  all_data_[guid] = std::move(specifics);
}

bool DeviceInfoService::DeleteSpecifics(const std::string& guid,
                                        WriteBatch* batch) {
  ClientIdToSpecifics::const_iterator iter = all_data_.find(guid);
  if (iter != all_data_.end()) {
    DVLOG(1) << "Deleting DEVICE_INFO for " << iter->second->client_name()
             << " with ID " << guid;
    store_->DeleteData(batch, guid);
    all_data_.erase(iter);
    return true;
  } else {
    return false;
  }
}

void DeviceInfoService::OnProviderInitialized() {
  has_provider_initialized_ = true;
  TryReconcileLocalAndStored();
}

void DeviceInfoService::OnStoreCreated(Result result,
                                       std::unique_ptr<ModelTypeStore> store) {
  if (result == Result::SUCCESS) {
    std::swap(store_, store);
    store_->ReadAllData(base::Bind(&DeviceInfoService::OnReadAllData,
                                   weak_factory_.GetWeakPtr()));
  } else {
    LOG(WARNING) << "ModelTypeStore creation failed.";
    // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
    // failure.
  }
}

void DeviceInfoService::OnReadAllData(Result result,
                                      std::unique_ptr<RecordList> record_list) {
  if (result != Result::SUCCESS) {
    LOG(WARNING) << "Initial load of data failed.";
    // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
    // failure.
    return;
  }

  for (const Record& r : *record_list.get()) {
    std::unique_ptr<DeviceInfoSpecifics> specifics(
        base::WrapUnique(new DeviceInfoSpecifics()));
    if (specifics->ParseFromString(r.value)) {
      all_data_[specifics->cache_guid()] = std::move(specifics);
    } else {
      LOG(WARNING) << "Failed to deserialize specifics.";
      // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
      // failure.
    }
  }
  store_->ReadAllMetadata(base::Bind(&DeviceInfoService::OnReadAllMetadata,
                                     weak_factory_.GetWeakPtr()));
}

void DeviceInfoService::OnReadAllMetadata(
    Result result,
    std::unique_ptr<RecordList> metadata_records,
    const std::string& global_metadata) {
  if (result != Result::SUCCESS) {
    // Store has encountered some serious error. We should still be able to
    // continue as a read only service, since if we got this far we must have
    // loaded all data out succesfully. TODO(skym): Should we communicate this
    // to sync somehow?
    LOG(WARNING) << "Load of metadata completely failed.";
    return;
  }

  // If we have no metadata then we don't want to create a processor. The idea
  // is that by not having a processor, the services will suffer less of a
  // performance hit. This isn't terribly applicable for this model type, but
  // we want this class to be as similar to other services as possible so follow
  // the convention.
  if (metadata_records->size() > 0 || !global_metadata.empty()) {
    CreateChangeProcessor();
  }

  // Set this after OnChangeProcessorSet so that we can correctly avoid giving
  // the processor empty metadata. We always want to set |has_metadata_loaded_|
  // at this point so that we'll know to give a processor empty metadata if it
  // is created later.
  has_metadata_loaded_ = true;

  if (!change_processor()) {
    // This means we haven't been told to start sycning and we don't have any
    // local metadata
    return;
  }

  std::unique_ptr<MetadataBatch> batch(new MetadataBatch());
  DataTypeState state;
  if (state.ParseFromString(global_metadata)) {
    batch->SetDataTypeState(state);
  } else {
    // TODO(skym): How bad is this scenario? We may be able to just give an
    // empty batch to the processor and we'll treat corrupted data type state
    // as no data type state at all. The question is do we want to add any of
    // the entity metadata to the batch or completely skip that step? We're
    // going to have to perform a merge shortly. Does this decision/logic even
    // belong in this service?
    LOG(WARNING) << "Failed to deserialize global metadata.";
  }
  for (const Record& r : *metadata_records.get()) {
    sync_pb::EntityMetadata entity_metadata;
    if (entity_metadata.ParseFromString(r.value)) {
      batch->AddMetadata(r.id, entity_metadata);
    } else {
      // TODO(skym): This really isn't too bad. We just want to regenerate
      // metadata for this particular entity. Unfortunately there isn't a
      // convinient way to tell the processor to do this.
      LOG(WARNING) << "Failed to deserialize entity metadata.";
    }
  }
  change_processor()->OnMetadataLoaded(std::move(batch));
  TryReconcileLocalAndStored();
}

void DeviceInfoService::OnCommit(Result result) {
  if (result != Result::SUCCESS) {
    LOG(WARNING) << "Failed a write to store.";
  }
}

void DeviceInfoService::TryReconcileLocalAndStored() {
  // On initial syncing we will have a change processor here, but it will not be
  // tracking changes. We need to persist a copy of our local device info to
  // disk, but the Put call to the processor will be ignored. That should be
  // fine however, as the discrepancy will be picked up later in merge. We don't
  // bother trying to track this case and act intelligently because simply not
  // much of a benefit in doing so.
  if (has_provider_initialized_ && has_metadata_loaded_ && change_processor()) {
    const DeviceInfo* current_info =
        local_device_info_provider_->GetLocalDeviceInfo();
    auto iter = all_data_.find(current_info->guid());
    // Convert to DeviceInfo for Equals function.
    if (iter == all_data_.end() ||
        !current_info->Equals(*CopyToModel(*iter->second))) {
      PutAndStore(*current_info);
    }
  }
}

void DeviceInfoService::PutAndStore(const DeviceInfo& device_info) {
  std::unique_ptr<DeviceInfoSpecifics> specifics = CopyToSpecifics(device_info);

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  change_processor()->Put(SpecificsToTag(*specifics),
                          CopyToEntityData(*specifics),
                          metadata_change_list.get());

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  StoreSpecifics(std::move(specifics), batch.get());

  CommitAndNotify(std::move(batch), std::move(metadata_change_list), true);
}

void DeviceInfoService::CommitAndNotify(
    std::unique_ptr<WriteBatch> batch,
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    bool should_notify) {
  static_cast<SimpleMetadataChangeList*>(metadata_change_list.get())
      ->TransferChanges(store_.get(), batch.get());
  store_->CommitWriteBatch(
      std::move(batch),
      base::Bind(&DeviceInfoService::OnCommit, weak_factory_.GetWeakPtr()));
  if (should_notify) {
    NotifyObservers();
  }
}

}  // namespace sync_driver_v2
