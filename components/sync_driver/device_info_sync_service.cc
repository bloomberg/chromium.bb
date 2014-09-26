// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_sync_service.h"

#include "base/strings/stringprintf.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "sync/api/sync_change.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/time.h"

namespace sync_driver {

using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

DeviceInfoSyncService::DeviceInfoSyncService(
    LocalDeviceInfoProvider* local_device_info_provider)
    : local_device_backup_time_(-1),
      local_device_info_provider_(local_device_info_provider) {
  DCHECK(local_device_info_provider);
}

DeviceInfoSyncService::~DeviceInfoSyncService() {
}

SyncMergeResult DeviceInfoSyncService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    scoped_ptr<SyncChangeProcessor> sync_processor,
    scoped_ptr<SyncErrorFactory> error_handler) {
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());
  DCHECK_EQ(type, syncer::DEVICE_INFO);

  DCHECK(all_data_.empty());

  sync_processor_ = sync_processor.Pass();
  error_handler_ = error_handler.Pass();

  // Initialization should be completed before this type is enabled
  // and local device info must be available.
  const DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  DCHECK(local_device_info != NULL);

  // Indicates whether a local device has been added or updated.
  // |change_type| defaults to ADD and might be changed to
  // UPDATE to INVALID down below if the initial data contains
  // data matching the local device ID.
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_ADD;
  size_t num_items_new = 0;
  size_t num_items_updated = 0;

  // Iterate over all initial sync data and copy it to the cache.
  for (SyncDataList::const_iterator iter = initial_sync_data.begin();
       iter != initial_sync_data.end();
       ++iter) {
    DCHECK_EQ(syncer::DEVICE_INFO, iter->GetDataType());

    const std::string& id = iter->GetSpecifics().device_info().cache_guid();

    if (id == local_device_info->guid()) {
      // |initial_sync_data| contains data matching the local device.
      scoped_ptr<DeviceInfo> synced_local_device_info =
          make_scoped_ptr(CreateDeviceInfo(*iter));

      // Retrieve local device backup timestamp value from the sync data.
      bool has_synced_backup_time =
          iter->GetSpecifics().device_info().has_backup_timestamp();
      int64 synced_backup_time =
          has_synced_backup_time
              ? iter->GetSpecifics().device_info().backup_timestamp()
              : -1;

      // Overwrite |local_device_backup_time_| with this value if it
      // hasn't been set yet.
      if (!has_local_device_backup_time() && has_synced_backup_time) {
        set_local_device_backup_time(synced_backup_time);
      }

      // Store the synced device info for the local device only
      // it is the same as the local info. Otherwise store the local
      // device info and issue a change further below after finishing
      // processing the |initial_sync_data|.
      if (synced_local_device_info->Equals(*local_device_info) &&
          synced_backup_time == local_device_backup_time()) {
        change_type = SyncChange::ACTION_INVALID;
      } else {
        num_items_updated++;
        change_type = SyncChange::ACTION_UPDATE;
        continue;
      }
    } else {
      // A new device that doesn't match the local device.
      num_items_new++;
    }

    StoreSyncData(id, *iter);
  }

  syncer::SyncMergeResult result(type);

  // Add SyncData for the local device if it is new or different than
  // the synced one, and also add it to the |change_list|.
  if (change_type != SyncChange::ACTION_INVALID) {
    SyncData local_data = CreateLocalData(local_device_info);
    StoreSyncData(local_device_info->guid(), local_data);

    SyncChangeList change_list;
    change_list.push_back(SyncChange(FROM_HERE, change_type, local_data));
    result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));
  }

  result.set_num_items_before_association(1);
  result.set_num_items_after_association(all_data_.size());
  result.set_num_items_added(num_items_new);
  result.set_num_items_modified(num_items_updated);
  result.set_num_items_deleted(0);

  NotifyObservers();

  return result;
}

void DeviceInfoSyncService::StopSyncing(syncer::ModelType type) {
  all_data_.clear();
  sync_processor_.reset();
  error_handler_.reset();
  clear_local_device_backup_time();
}

SyncDataList DeviceInfoSyncService::GetAllSyncData(
    syncer::ModelType type) const {
  SyncDataList list;

  for (SyncDataMap::const_iterator iter = all_data_.begin();
       iter != all_data_.end();
       ++iter) {
    list.push_back(iter->second);
  }

  return list;
}

syncer::SyncError DeviceInfoSyncService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  syncer::SyncError error;

  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());
  const std::string& local_device_id =
      local_device_info_provider_->GetLocalDeviceInfo()->guid();

  bool has_changes = false;

  // Iterate over all chanages and merge entries.
  for (SyncChangeList::const_iterator iter = change_list.begin();
       iter != change_list.end();
       ++iter) {
    const SyncData& sync_data = iter->sync_data();
    DCHECK_EQ(syncer::DEVICE_INFO, sync_data.GetDataType());

    const std::string& client_id =
        sync_data.GetSpecifics().device_info().cache_guid();
    // Ignore device info matching the local device.
    if (local_device_id == client_id) {
      DVLOG(1) << "Ignoring sync changes for the local DEVICE_INFO";
      continue;
    }

    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      has_changes = true;
      DeleteSyncData(client_id);
    } else if (iter->change_type() == syncer::SyncChange::ACTION_UPDATE ||
               iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      has_changes = true;
      StoreSyncData(client_id, sync_data);
    } else {
      error.Reset(FROM_HERE, "Invalid action received.", syncer::DEVICE_INFO);
    }
  }

  if (has_changes) {
    NotifyObservers();
  }

  return error;
}

scoped_ptr<DeviceInfo> DeviceInfoSyncService::GetDeviceInfo(
    const std::string& client_id) const {
  SyncDataMap::const_iterator iter = all_data_.find(client_id);
  if (iter == all_data_.end()) {
    return scoped_ptr<DeviceInfo>();
  }

  return make_scoped_ptr(CreateDeviceInfo(iter->second));
}

ScopedVector<DeviceInfo> DeviceInfoSyncService::GetAllDeviceInfo() const {
  ScopedVector<DeviceInfo> list;

  for (SyncDataMap::const_iterator iter = all_data_.begin();
       iter != all_data_.end();
       ++iter) {
    list.push_back(CreateDeviceInfo(iter->second));
  }

  return list.Pass();
}

void DeviceInfoSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceInfoSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceInfoSyncService::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceInfoChange());
}

void DeviceInfoSyncService::UpdateLocalDeviceBackupTime(
    base::Time backup_time) {
  set_local_device_backup_time(syncer::TimeToProtoTime(backup_time));

  if (sync_processor_.get()) {
    // Local device info must be available in advance
    DCHECK(local_device_info_provider_->GetLocalDeviceInfo());
    const std::string& local_id =
        local_device_info_provider_->GetLocalDeviceInfo()->guid();

    SyncDataMap::iterator iter = all_data_.find(local_id);
    DCHECK(iter != all_data_.end());

    syncer::SyncData& data = iter->second;
    if (UpdateBackupTime(&data)) {
      // Local device backup time has changed.
      // Push changes to the server via the |sync_processor_|.
      SyncChangeList change_list;
      change_list.push_back(SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_UPDATE, data));
      sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
    }
  }
}

bool DeviceInfoSyncService::UpdateBackupTime(syncer::SyncData* sync_data) {
  DCHECK(has_local_device_backup_time());
  DCHECK(sync_data->GetSpecifics().has_device_info());
  const sync_pb::DeviceInfoSpecifics& source_specifics =
      sync_data->GetSpecifics().device_info();

  if (!source_specifics.has_backup_timestamp() ||
      source_specifics.backup_timestamp() != local_device_backup_time()) {
    sync_pb::EntitySpecifics entity(sync_data->GetSpecifics());
    entity.mutable_device_info()->set_backup_timestamp(
        local_device_backup_time());
    *sync_data = CreateLocalData(entity);

    return true;
  }

  return false;
}

base::Time DeviceInfoSyncService::GetLocalDeviceBackupTime() const {
  return has_local_device_backup_time()
             ? syncer::ProtoTimeToTime(local_device_backup_time())
             : base::Time();
}

SyncData DeviceInfoSyncService::CreateLocalData(const DeviceInfo* info) {
  sync_pb::EntitySpecifics entity;
  sync_pb::DeviceInfoSpecifics& specifics = *entity.mutable_device_info();

  specifics.set_cache_guid(info->guid());
  specifics.set_client_name(info->client_name());
  specifics.set_chrome_version(info->chrome_version());
  specifics.set_sync_user_agent(info->sync_user_agent());
  specifics.set_device_type(info->device_type());
  specifics.set_signin_scoped_device_id(info->signin_scoped_device_id());

  if (has_local_device_backup_time()) {
    specifics.set_backup_timestamp(local_device_backup_time());
  }

  return CreateLocalData(entity);
}

SyncData DeviceInfoSyncService::CreateLocalData(
    const sync_pb::EntitySpecifics& entity) {
  const sync_pb::DeviceInfoSpecifics& specifics = entity.device_info();

  std::string local_device_tag =
      base::StringPrintf("DeviceInfo_%s", specifics.cache_guid().c_str());

  return SyncData::CreateLocalData(
      local_device_tag, specifics.client_name(), entity);
}

DeviceInfo* DeviceInfoSyncService::CreateDeviceInfo(
    const syncer::SyncData sync_data) {
  const sync_pb::DeviceInfoSpecifics& specifics =
      sync_data.GetSpecifics().device_info();

  return new DeviceInfo(specifics.cache_guid(),
                        specifics.client_name(),
                        specifics.chrome_version(),
                        specifics.sync_user_agent(),
                        specifics.device_type(),
                        specifics.signin_scoped_device_id());
}

void DeviceInfoSyncService::StoreSyncData(const std::string& client_id,
                                          const SyncData& sync_data) {
  DVLOG(1) << "Storing DEVICE_INFO for "
           << sync_data.GetSpecifics().device_info().client_name()
           << " with ID " << client_id;
  all_data_[client_id] = sync_data;
}

void DeviceInfoSyncService::DeleteSyncData(const std::string& client_id) {
  SyncDataMap::iterator iter = all_data_.find(client_id);
  if (iter != all_data_.end()) {
    DVLOG(1) << "Deleting DEVICE_INFO for "
             << iter->second.GetSpecifics().device_info().client_name()
             << " with ID " << client_id;
    all_data_.erase(iter);
  }
}

}  // namespace sync_driver
