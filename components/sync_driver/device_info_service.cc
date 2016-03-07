// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_service.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
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

DeviceInfoService::DeviceInfoService(
    sync_driver::LocalDeviceInfoProvider* local_device_info_provider,
    const StoreFactoryFunction& callback)
    : local_device_info_provider_(local_device_info_provider),
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

scoped_ptr<MetadataChangeList> DeviceInfoService::CreateMetadataChangeList() {
  return make_scoped_ptr(new SimpleMetadataChangeList());
}

SyncError DeviceInfoService::MergeSyncData(
    scoped_ptr<MetadataChangeList> metadata_change_list,
    EntityDataMap entity_data_map) {
  if (!has_provider_initialized_ || !has_data_loaded_ || !change_processor()) {
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
  std::set<std::string> local_only_tags;
  for (const auto& kv : all_data_) {
    local_only_tags.insert(kv.first);
  }

  bool has_changes = false;
  std::string local_tag =
      local_device_info_provider_->GetLocalDeviceInfo()->guid();
  scoped_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const auto& kv : entity_data_map) {
    const std::string tag = GetClientTag(kv.second.value());
    const DeviceInfoSpecifics& specifics =
        kv.second.value().specifics.device_info();

    // Ignore any remote changes that have our local cache guid.
    if (tag == local_tag) {
      continue;
    }

    // Remote data wins conflicts.
    local_only_tags.erase(tag);
    StoreSpecifics(make_scoped_ptr(new DeviceInfoSpecifics(specifics)),
                   batch.get());
    has_changes = true;
  }

  for (const std::string& tag : local_only_tags) {
    change_processor()->Put(tag, CopyIntoNewEntityData(*all_data_[tag]),
                            metadata_change_list.get());
  }

  // Transfer at the end because processor Put calls may update metadata.
  static_cast<SimpleMetadataChangeList*>(metadata_change_list.get())
      ->TransferChanges(store_.get(), batch.get());
  store_->CommitWriteBatch(
      std::move(batch),
      base::Bind(&DeviceInfoService::OnCommit, weak_factory_.GetWeakPtr()));
  if (has_changes) {
    NotifyObservers();
  }
  return syncer::SyncError();
}

SyncError DeviceInfoService::ApplySyncChanges(
    scoped_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  if (!has_provider_initialized_ || !has_data_loaded_) {
    return SyncError(
        FROM_HERE, SyncError::DATATYPE_ERROR,
        "Cannot call ApplySyncChanges before provider and data have loaded.",
        syncer::DEVICE_INFO);
  }

  scoped_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  static_cast<SimpleMetadataChangeList*>(metadata_change_list.get())
      ->TransferChanges(store_.get(), batch.get());
  bool has_changes = false;
  for (EntityChange& change : entity_changes) {
    const std::string tag = change.client_tag();
    // Each device is the authoritative source for itself, ignore any remote
    // changes that have our local cache guid.
    if (tag == local_device_info_provider_->GetLocalDeviceInfo()->guid()) {
      continue;
    }

    if (change.type() == EntityChange::ACTION_DELETE) {
      has_changes |= DeleteSpecifics(tag, batch.get());
    } else {
      const DeviceInfoSpecifics& specifics =
          change.data().specifics.device_info();
      DCHECK(tag == specifics.cache_guid());
      StoreSpecifics(make_scoped_ptr(new DeviceInfoSpecifics(specifics)),
                     batch.get());
      has_changes = true;
    }
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::Bind(&DeviceInfoService::OnCommit, weak_factory_.GetWeakPtr()));
  if (has_changes) {
    NotifyObservers();
  }
  return SyncError();
}

void DeviceInfoService::GetData(ClientTagList client_tags,
                                DataCallback callback) {
  if (!has_data_loaded_) {
    callback.Run(SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                           "Cannot call GetData before data has loaded.",
                           syncer::DEVICE_INFO),
                 scoped_ptr<DataBatchImpl>());
    return;
  }

  syncer::SyncError error;
  scoped_ptr<DataBatchImpl> batch(new DataBatchImpl());
  for (const auto& tag : client_tags) {
    const auto iter = all_data_.find(tag);
    if (iter != all_data_.end()) {
      batch->Put(tag, CopyIntoNewEntityData(*iter->second));
    }
  }
  callback.Run(error, std::move(batch));
}

void DeviceInfoService::GetAllData(DataCallback callback) {
  if (!has_data_loaded_) {
    callback.Run(SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                           "Cannot call GetAllData before data has loaded.",
                           syncer::DEVICE_INFO),
                 scoped_ptr<DataBatchImpl>());
    return;
  }

  syncer::SyncError error;
  scoped_ptr<DataBatchImpl> batch(new DataBatchImpl());
  for (const auto& kv : all_data_) {
    batch->Put(kv.first, CopyIntoNewEntityData(*kv.second));
  }
  callback.Run(error, std::move(batch));
}

std::string DeviceInfoService::GetClientTag(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return entity_data.specifics.device_info().cache_guid();
}

void DeviceInfoService::OnChangeProcessorSet() {
  TryLoadAllMetadata();
}

bool DeviceInfoService::IsSyncing() const {
  return !all_data_.empty();
}

scoped_ptr<DeviceInfo> DeviceInfoService::GetDeviceInfo(
    const std::string& client_id) const {
  const ClientIdToSpecifics::const_iterator iter = all_data_.find(client_id);
  if (iter == all_data_.end()) {
    return scoped_ptr<DeviceInfo>();
  }

  return CreateDeviceInfo(*iter->second);
}

ScopedVector<DeviceInfo> DeviceInfoService::GetAllDeviceInfo() const {
  ScopedVector<DeviceInfo> list;

  for (ClientIdToSpecifics::const_iterator iter = all_data_.begin();
       iter != all_data_.end(); ++iter) {
    list.push_back(CreateDeviceInfo(*iter->second));
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

// TODO(skym): crbug.com/543406: It might not make sense for this to be a
// scoped_ptr.
scoped_ptr<DeviceInfoSpecifics> DeviceInfoService::CreateLocalSpecifics() {
  const DeviceInfo* info = local_device_info_provider_->GetLocalDeviceInfo();
  DCHECK(info);
  scoped_ptr<DeviceInfoSpecifics> specifics = CreateSpecifics(*info);
  // TODO(skym): crbug.com:543406: Local tag and non unique name have no place
  // to be set now.
  return specifics;
}

// TODO(skym): crbug.com/543406: It might not make sense for this to be a
// scoped_ptr.
// Static.
scoped_ptr<DeviceInfoSpecifics> DeviceInfoService::CreateSpecifics(
    const DeviceInfo& info) {
  scoped_ptr<DeviceInfoSpecifics> specifics =
      make_scoped_ptr(new DeviceInfoSpecifics);
  specifics->set_cache_guid(info.guid());
  specifics->set_client_name(info.client_name());
  specifics->set_chrome_version(info.chrome_version());
  specifics->set_sync_user_agent(info.sync_user_agent());
  specifics->set_device_type(info.device_type());
  specifics->set_signin_scoped_device_id(info.signin_scoped_device_id());
  return specifics;
}

// Static.
scoped_ptr<DeviceInfo> DeviceInfoService::CreateDeviceInfo(
    const DeviceInfoSpecifics& specifics) {
  return make_scoped_ptr(new DeviceInfo(
      specifics.cache_guid(), specifics.client_name(),
      specifics.chrome_version(), specifics.sync_user_agent(),
      specifics.device_type(), specifics.signin_scoped_device_id()));
}

// Static.
scoped_ptr<EntityData> DeviceInfoService::CopyIntoNewEntityData(
    const DeviceInfoSpecifics& specifics) {
  scoped_ptr<EntityData> entity_data(new EntityData());
  *entity_data->specifics.mutable_device_info() = specifics;
  return entity_data;
}

void DeviceInfoService::StoreSpecifics(
    scoped_ptr<DeviceInfoSpecifics> specifics,
    WriteBatch* batch) {
  const std::string tag = specifics->cache_guid();
  DVLOG(1) << "Storing DEVICE_INFO for " << specifics->client_name()
           << " with ID " << tag;
  store_->WriteData(batch, tag, specifics->SerializeAsString());
  all_data_[tag] = std::move(specifics);
}

bool DeviceInfoService::DeleteSpecifics(const std::string& tag,
                                        WriteBatch* batch) {
  ClientIdToSpecifics::const_iterator iter = all_data_.find(tag);
  if (iter != all_data_.end()) {
    DVLOG(1) << "Deleting DEVICE_INFO for " << iter->second->client_name()
             << " with ID " << tag;
    store_->DeleteData(batch, tag);
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
                                       scoped_ptr<ModelTypeStore> store) {
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
                                      scoped_ptr<RecordList> record_list) {
  if (result == Result::SUCCESS) {
    for (const Record& r : *record_list.get()) {
      scoped_ptr<DeviceInfoSpecifics> specifics(
          make_scoped_ptr(new DeviceInfoSpecifics()));
      if (specifics->ParseFromString(r.value)) {
        all_data_[r.id] = std::move(specifics);
      } else {
        LOG(WARNING) << "Failed to deserialize specifics.";
        // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
        // failure.
      }
    }
    has_data_loaded_ = true;
    TryLoadAllMetadata();
  } else {
    LOG(WARNING) << "Initial load of data failed.";
    // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
    // failure.
  }
}

void DeviceInfoService::OnReadAllMetadata(
    Result result,
    scoped_ptr<RecordList> metadata_records,
    const std::string& global_metadata) {
  if (!change_processor()) {
    // This datatype was disabled while this read was oustanding.
    return;
  }
  if (result != Result::SUCCESS) {
    // Store has encountered some serious error. We should still be able to
    // continue as a read only service, since if we got this far we must have
    // loaded all data out succesfully. TODO(skym): Should we communicate this
    // to sync somehow?
    LOG(WARNING) << "Load of metadata completely failed.";
    return;
  }
  scoped_ptr<MetadataBatch> batch(new MetadataBatch());
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
}

void DeviceInfoService::OnCommit(Result result) {
  if (result != Result::SUCCESS) {
    LOG(WARNING) << "Failed a write to store.";
  }
}

void DeviceInfoService::TryReconcileLocalAndStored() {
  // TODO(skym, crbug.com/582460): Implement logic to reconcile provider and
  // stored device infos.
}

void DeviceInfoService::TryLoadAllMetadata() {
  if (has_data_loaded_ && change_processor()) {
    store_->ReadAllMetadata(base::Bind(&DeviceInfoService::OnReadAllMetadata,
                                       weak_factory_.GetWeakPtr()));
  }
}

}  // namespace sync_driver_v2
