// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_service.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "components/sync/base/time.h"
#include "components/sync/device_info/device_info_util.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using base::Time;
using base::TimeDelta;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;

using Record = ModelTypeStore::Record;
using RecordList = ModelTypeStore::RecordList;
using Result = ModelTypeStore::Result;
using WriteBatch = ModelTypeStore::WriteBatch;

DeviceInfoService::DeviceInfoService(
    LocalDeviceInfoProvider* local_device_info_provider,
    const StoreFactoryFunction& callback,
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeService(change_processor_factory, DEVICE_INFO),
      local_device_info_provider_(local_device_info_provider) {
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

  callback.Run(
      base::Bind(&DeviceInfoService::OnStoreCreated, base::AsWeakPtr(this)));
}

DeviceInfoService::~DeviceInfoService() {}

std::unique_ptr<MetadataChangeList>
DeviceInfoService::CreateMetadataChangeList() {
  return base::MakeUnique<SimpleMetadataChangeList>();
}

SyncError DeviceInfoService::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityDataMap entity_data_map) {
  DCHECK(has_provider_initialized_);
  DCHECK(change_processor()->IsTrackingMetadata());

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
    DCHECK_EQ(kv.first, specifics.cache_guid());
    if (specifics.cache_guid() == local_guid) {
      // Don't Put local data if it's the same as the remote copy.
      if (local_info->Equals(*CopyToModel(specifics))) {
        local_guids_to_put.erase(local_guid);
      } else {
        // This device is valid right now and this entry is about to be
        // committed, use this as an opportunity to refresh the timestamp.
        all_data_[local_guid]->set_last_updated_timestamp(
            TimeToProtoTime(Time::Now()));
      }
    } else {
      // Remote data wins conflicts.
      local_guids_to_put.erase(specifics.cache_guid());
      has_changes = true;
      StoreSpecifics(base::MakeUnique<DeviceInfoSpecifics>(specifics),
                     batch.get());
    }
  }

  for (const std::string& guid : local_guids_to_put) {
    change_processor()->Put(guid, CopyToEntityData(*all_data_[guid]),
                            metadata_change_list.get());
  }

  CommitAndNotify(std::move(batch), std::move(metadata_change_list),
                  has_changes);
  return SyncError();
}

SyncError DeviceInfoService::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  DCHECK(has_provider_initialized_);

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  bool has_changes = false;
  for (EntityChange& change : entity_changes) {
    const std::string guid = change.storage_key();
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
      StoreSpecifics(base::MakeUnique<DeviceInfoSpecifics>(specifics),
                     batch.get());
      has_changes = true;
    }
  }

  CommitAndNotify(std::move(batch), std::move(metadata_change_list),
                  has_changes);
  return SyncError();
}

void DeviceInfoService::GetData(StorageKeyList storage_keys,
                                DataCallback callback) {
  auto batch = base::MakeUnique<MutableDataBatch>();
  for (const auto& key : storage_keys) {
    const auto& iter = all_data_.find(key);
    if (iter != all_data_.end()) {
      DCHECK_EQ(key, iter->second->cache_guid());
      batch->Put(key, CopyToEntityData(*iter->second));
    }
  }
  callback.Run(SyncError(), std::move(batch));
}

void DeviceInfoService::GetAllData(DataCallback callback) {
  auto batch = base::MakeUnique<MutableDataBatch>();
  for (const auto& kv : all_data_) {
    batch->Put(kv.first, CopyToEntityData(*kv.second));
  }
  callback.Run(SyncError(), std::move(batch));
}

std::string DeviceInfoService::GetClientTag(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return DeviceInfoUtil::SpecificsToTag(entity_data.specifics.device_info());
}

std::string DeviceInfoService::GetStorageKey(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return entity_data.specifics.device_info().cache_guid();
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

std::vector<std::unique_ptr<DeviceInfo>> DeviceInfoService::GetAllDeviceInfo()
    const {
  std::vector<std::unique_ptr<DeviceInfo>> list;
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
  return CountActiveDevices(Time::Now());
}

void DeviceInfoService::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnDeviceInfoChange();
}

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
  return base::MakeUnique<DeviceInfo>(
      specifics.cache_guid(), specifics.client_name(),
      specifics.chrome_version(), specifics.sync_user_agent(),
      specifics.device_type(), specifics.signin_scoped_device_id());
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
  // Now that the provider has initialized, remove the subscription. The service
  // should only need to give the processor metadata upon initialization. If
  // sync is disabled and enabled, our provider will try to retrigger this
  // event, but we do not want to send any more metadata to the processor.
  subscription_.reset();

  has_provider_initialized_ = true;
  LoadMetadataIfReady();
}

void DeviceInfoService::OnStoreCreated(Result result,
                                       std::unique_ptr<ModelTypeStore> store) {
  if (result == Result::SUCCESS) {
    std::swap(store_, store);
    store_->ReadAllData(
        base::Bind(&DeviceInfoService::OnReadAllData, base::AsWeakPtr(this)));
  } else {
    ReportStartupErrorToSync("ModelTypeStore creation failed.");
    // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
    // failure.
  }
}

void DeviceInfoService::OnReadAllData(Result result,
                                      std::unique_ptr<RecordList> record_list) {
  if (result != Result::SUCCESS) {
    ReportStartupErrorToSync("Initial load of data failed.");
    // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
    // failure.
    return;
  }

  for (const Record& r : *record_list.get()) {
    std::unique_ptr<DeviceInfoSpecifics> specifics =
        base::MakeUnique<DeviceInfoSpecifics>();
    if (specifics->ParseFromString(r.value)) {
      all_data_[specifics->cache_guid()] = std::move(specifics);
    } else {
      ReportStartupErrorToSync("Failed to deserialize specifics.");
      // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
      // failure.
    }
  }

  has_data_loaded_ = true;
  LoadMetadataIfReady();
}

void DeviceInfoService::LoadMetadataIfReady() {
  if (has_data_loaded_ && has_provider_initialized_) {
    store_->ReadAllMetadata(base::Bind(&DeviceInfoService::OnReadAllMetadata,
                                       base::AsWeakPtr(this)));
  }
}

void DeviceInfoService::OnReadAllMetadata(
    Result result,
    std::unique_ptr<RecordList> metadata_records,
    const std::string& global_metadata) {
  if (result != Result::SUCCESS) {
    // Store has encountered some serious error. We should still be able to
    // continue as a read only service, since if we got this far we must have
    // loaded all data out succesfully.
    ReportStartupErrorToSync("Load of metadata completely failed.");
    return;
  }

  std::unique_ptr<MetadataBatch> batch(new MetadataBatch());
  ModelTypeState state;
  if (state.ParseFromString(global_metadata)) {
    batch->SetModelTypeState(state);
  } else {
    // TODO(skym): How bad is this scenario? We may be able to just give an
    // empty batch to the processor and we'll treat corrupted data type state
    // as no data type state at all. The question is do we want to add any of
    // the entity metadata to the batch or completely skip that step? We're
    // going to have to perform a merge shortly. Does this decision/logic even
    // belong in this service?
    change_processor()->OnMetadataLoaded(
        change_processor()->CreateAndUploadError(
            FROM_HERE, "Failed to deserialize global metadata."),
        nullptr);
  }
  for (const Record& r : *metadata_records.get()) {
    sync_pb::EntityMetadata entity_metadata;
    if (entity_metadata.ParseFromString(r.value)) {
      batch->AddMetadata(r.id, entity_metadata);
    } else {
      // TODO(skym): This really isn't too bad. We just want to regenerate
      // metadata for this particular entity. Unfortunately there isn't a
      // convenient way to tell the processor to do this.
      LOG(WARNING) << "Failed to deserialize entity metadata.";
    }
  }
  change_processor()->OnMetadataLoaded(SyncError(), std::move(batch));
  ReconcileLocalAndStored();
}

void DeviceInfoService::OnCommit(Result result) {
  if (result != Result::SUCCESS) {
    LOG(WARNING) << "Failed a write to store.";
  }
}

void DeviceInfoService::ReconcileLocalAndStored() {
  // On initial syncing we will have a change processor here, but it will not be
  // tracking changes. We need to persist a copy of our local device info to
  // disk, but the Put call to the processor will be ignored. That should be
  // fine however, as the discrepancy will be picked up later in merge. We don't
  // bother trying to track this case and act intelligently because simply not
  // much of a benefit in doing so.
  DCHECK(has_provider_initialized_);

  const DeviceInfo* current_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  auto iter = all_data_.find(current_info->guid());

  // Convert to DeviceInfo for Equals function.
  if (iter != all_data_.end() &&
      current_info->Equals(*CopyToModel(*iter->second))) {
    const TimeDelta pulse_delay(DeviceInfoUtil::CalculatePulseDelay(
        GetLastUpdateTime(*iter->second), Time::Now()));
    if (!pulse_delay.is_zero()) {
      pulse_timer_.Start(FROM_HERE, pulse_delay,
                         base::Bind(&DeviceInfoService::SendLocalData,
                                    base::Unretained(this)));
      return;
    }
  }
  SendLocalData();
}

void DeviceInfoService::SendLocalData() {
  DCHECK(has_provider_initialized_);

  // It is possible that the provider no longer has data for us, such as when
  // the user signs out. No-op this pulse, but keep the timer going in case sync
  // is enabled later.
  if (local_device_info_provider_->GetLocalDeviceInfo() != nullptr) {
    std::unique_ptr<DeviceInfoSpecifics> specifics =
        CopyToSpecifics(*local_device_info_provider_->GetLocalDeviceInfo());
    specifics->set_last_updated_timestamp(TimeToProtoTime(Time::Now()));

    std::unique_ptr<MetadataChangeList> metadata_change_list =
        CreateMetadataChangeList();
    if (change_processor()->IsTrackingMetadata()) {
      change_processor()->Put(specifics->cache_guid(),
                              CopyToEntityData(*specifics),
                              metadata_change_list.get());
    }

    std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
    StoreSpecifics(std::move(specifics), batch.get());
    CommitAndNotify(std::move(batch), std::move(metadata_change_list), true);
  }

  pulse_timer_.Start(
      FROM_HERE, DeviceInfoUtil::kPulseInterval,
      base::Bind(&DeviceInfoService::SendLocalData, base::Unretained(this)));
}

void DeviceInfoService::CommitAndNotify(
    std::unique_ptr<WriteBatch> batch,
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    bool should_notify) {
  static_cast<SimpleMetadataChangeList*>(metadata_change_list.get())
      ->TransferChanges(store_.get(), batch.get());
  store_->CommitWriteBatch(
      std::move(batch),
      base::Bind(&DeviceInfoService::OnCommit, base::AsWeakPtr(this)));
  if (should_notify) {
    NotifyObservers();
  }
}

int DeviceInfoService::CountActiveDevices(const Time now) const {
  return std::count_if(all_data_.begin(), all_data_.end(),
                       [now](ClientIdToSpecifics::const_reference pair) {
                         return DeviceInfoUtil::IsActive(
                             GetLastUpdateTime(*pair.second), now);
                       });
}

void DeviceInfoService::ReportStartupErrorToSync(const std::string& msg) {
  // TODO(skym): Shouldn't need to log this here, reporting should always log.
  LOG(WARNING) << msg;
  change_processor()->OnMetadataLoaded(
      change_processor()->CreateAndUploadError(FROM_HERE, msg), nullptr);
}

// static
Time DeviceInfoService::GetLastUpdateTime(
    const DeviceInfoSpecifics& specifics) {
  if (specifics.has_last_updated_timestamp()) {
    return ProtoTimeToTime(specifics.last_updated_timestamp());
  } else {
    return Time();
  }
}

}  // namespace syncer
