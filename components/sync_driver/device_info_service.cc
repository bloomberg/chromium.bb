// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_service.h"

#include "base/bind.h"
#include "sync/api/model_type_change_processor.h"
#include "sync/api/sync_error.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/time.h"

namespace sync_driver_v2 {

using sync_driver::DeviceInfo;
using sync_pb::DeviceInfoSpecifics;

DeviceInfoService::DeviceInfoService(
    sync_driver::LocalDeviceInfoProvider* local_device_info_provider)
    : local_device_backup_time_(-1),
      local_device_info_provider_(local_device_info_provider) {
  DCHECK(local_device_info_provider);

  if (local_device_info_provider->GetLocalDeviceInfo()) {
    OnProviderInitialized();
  } else {
    subscription_ =
        local_device_info_provider->RegisterOnInitializedCallback(base::Bind(
            &DeviceInfoService::OnProviderInitialized, base::Unretained(this)));
  }
}

DeviceInfoService::~DeviceInfoService() {}

syncer::SyncError DeviceInfoService::ApplySyncChanges() {
  // TODO(skym): For every model change, if not local, apply to memory + disk.
  // TODO(skym): For ever entity metadata change, apply to disk.
  // TODO(skym): Apply type metadata to disk.
  return syncer::SyncError();
}

syncer::SyncError DeviceInfoService::LoadMetadata() {
  // TODO(skym): Read out metadata from disk.
  return syncer::SyncError();
}

syncer::SyncError DeviceInfoService::UpdateMetadata() {
  // TODO(skym): Persist metadata to disk.
  return syncer::SyncError();
}

syncer::SyncError DeviceInfoService::GetData() {
  // TODO(skym): This is tricky. We're currently indexing data in memory by
  // cache_guid, not client tags. And we cannot change this, because they're not
  // equal on old clients. So maybe O(n*m) iterating through our map for every
  // peice of data we were asked for? Alternative we could make a set, and do
  // O(n + m). Another approach would to have two maps, which one ruling
  // ownership. Or we could just read out of disk for this, instead of memory.
  return syncer::SyncError();
}

syncer::SyncError DeviceInfoService::GetAllData() {
  // TODO(skym): Return all data from memory, unless we're not initialized.
  return syncer::SyncError();
}

syncer::SyncError DeviceInfoService::ClearMetadata() {
  // We act a little differently than most sync services here. All functionality
  // is to be shutdown and stopped, and all data is to be deleted.

  bool was_syncing = IsSyncing();

  all_data_.clear();
  clear_local_device_backup_time();
  clear_change_processor();

  if (was_syncing) {
    NotifyObservers();
  }

  // TODO(skym): Remove all data that's been persisted to storage.
  // TODO(skym): Somehow remember we're no longer intialize.

  return syncer::SyncError();
}

syncer_v2::ModelTypeChangeProcessor* DeviceInfoService::get_change_processor() {
  return change_processor_.get();
}

void DeviceInfoService::set_change_processor(
    scoped_ptr<syncer_v2::ModelTypeChangeProcessor> change_processor) {
  change_processor_.swap(change_processor);
}

void DeviceInfoService::clear_change_processor() {
  change_processor_.reset();
}

bool DeviceInfoService::IsSyncing() const {
  return !all_data_.empty();
}

scoped_ptr<DeviceInfo> DeviceInfoService::GetDeviceInfo(
    const std::string& client_id) const {
  ClientIdToSpecifics::const_iterator iter = all_data_.find(client_id);
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

  return list.Pass();
}

void DeviceInfoService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceInfoService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceInfoService::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceInfoChange());
}

void DeviceInfoService::UpdateLocalDeviceBackupTime(base::Time backup_time) {
  // TODO(skym): Replace with is initialized check, we've already started
  // syncing, provider is ready, make sure we have processort, etc.

  // Local device info must be available in advance.
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());

  // TODO(skym): Should this be a less than instead of not equal check?
  if (GetLocalDeviceBackupTime() != backup_time) {
    // TODO(skym): Storing this field doesn't really make sense, remove.
    set_local_device_backup_time(syncer::TimeToProtoTime(backup_time));
    scoped_ptr<DeviceInfoSpecifics> new_specifics = CreateLocalSpecifics();

    // TODO(skym): Create correct update datastructure, such as EntityChange,
    // EntityMetadata, or  CommitRequestData.
    // TODO(skym): Call ProcessChanges on SMTP.
    // TODO(skym): Persist metadata and data.
    StoreSpecifics(new_specifics.Pass());
  }

  // Don't call NotifyObservers() because backup time is not part of
  // DeviceInfoTracker interface.
}

base::Time DeviceInfoService::GetLocalDeviceBackupTime() const {
  return has_local_device_backup_time()
             ? syncer::ProtoTimeToTime(local_device_backup_time())
             : base::Time();
}

// TODO(skym): It might not make sense for this to be a scoped_ptr.
scoped_ptr<DeviceInfoSpecifics> DeviceInfoService::CreateLocalSpecifics() {
  const DeviceInfo* info = local_device_info_provider_->GetLocalDeviceInfo();
  DCHECK(info);
  scoped_ptr<DeviceInfoSpecifics> specifics = CreateSpecifics(*info);
  if (has_local_device_backup_time()) {
    specifics->set_backup_timestamp(local_device_backup_time());
  }
  // TODO(skym): Local tag and non unique name have no place to be set now.
  return specifics;
}

// TODO(skym): It might not make sense for this to be a scoped_ptr.
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

void DeviceInfoService::StoreSpecifics(
    scoped_ptr<DeviceInfoSpecifics> specifics) {
  DVLOG(1) << "Storing DEVICE_INFO for " << specifics->client_name()
           << " with ID " << specifics->cache_guid();
  const std::string& key = specifics->cache_guid();
  all_data_.set(key, specifics.Pass());
}

void DeviceInfoService::DeleteSpecifics(const std::string& client_id) {
  ClientIdToSpecifics::const_iterator iter = all_data_.find(client_id);
  if (iter != all_data_.end()) {
    DVLOG(1) << "Deleting DEVICE_INFO for " << iter->second->client_name()
             << " with ID " << client_id;
    all_data_.erase(iter);
  }
}

void DeviceInfoService::OnProviderInitialized() {
  // TODO(skym): Do we need this?
}

}  // namespace sync_driver_v2
