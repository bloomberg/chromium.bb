// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/device_info_sync_service.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/glue/local_device_info_provider.h"
#include "sync/api/sync_change.h"
#include "sync/protocol/sync.pb.h"

namespace browser_sync {

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
    : local_device_info_provider_(local_device_info_provider) {
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

  // Iterate over all initial sync data and copy it to the cache.
  for (SyncDataList::const_iterator iter = initial_sync_data.begin();
       iter != initial_sync_data.end();
       ++iter) {
    DCHECK_EQ(syncer::DEVICE_INFO, iter->GetDataType());
    StoreSyncData(iter->GetSpecifics().device_info().cache_guid(), *iter);
  }

  size_t num_items_new = initial_sync_data.size();
  size_t num_items_updated = 0;
  // Indicates whether a local device has been added or updated.
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_INVALID;

  // Initialization should be completed before this type is enabled
  // and local device info must be available.
  const DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  DCHECK(local_device_info != NULL);
  // Before storing the local device info check if the data with
  // the same guid has already been synced. This attempts to retrieve
  // DeviceInfo from the cached data initialized above.
  scoped_ptr<DeviceInfo> synced_local_device_info =
      GetDeviceInfo(local_device_info->guid());

  if (synced_local_device_info.get()) {
    // Local device info has been synced and exists in the cache.
    // |num_items_new| and |num_items_updated| need to be updated to
    // reflect that.
    num_items_new--;
    // Overwrite the synced device info with the local data only if
    // it is different.
    if (!synced_local_device_info->Equals(*local_device_info)) {
      num_items_updated++;
      change_type = SyncChange::ACTION_UPDATE;
    }
  } else {
    // Local device info doesn't yet exist in the cache and
    // will be added further below.
    // |num_items_new| and |num_items_updated| are already correct.
    change_type = SyncChange::ACTION_ADD;
  }

  syncer::SyncMergeResult result(type);

  // Update SyncData from device info if it is new or different than
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

SyncData DeviceInfoSyncService::CreateLocalData(const DeviceInfo* info) {
  sync_pb::EntitySpecifics entity;
  sync_pb::DeviceInfoSpecifics& specifics = *entity.mutable_device_info();

  specifics.set_cache_guid(info->guid());
  specifics.set_client_name(info->client_name());
  specifics.set_chrome_version(info->chrome_version());
  specifics.set_sync_user_agent(info->sync_user_agent());
  specifics.set_device_type(info->device_type());
  specifics.set_signin_scoped_device_id(info->signin_scoped_device_id());

  std::string local_device_tag =
      base::StringPrintf("DeviceInfo_%s", info->guid().c_str());

  return SyncData::CreateLocalData(
      local_device_tag, info->client_name(), entity);
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

}  // namespace browser_sync
