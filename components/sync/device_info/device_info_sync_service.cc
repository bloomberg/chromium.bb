// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_sync_service.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/sync/base/time.h"
#include "components/sync/device_info/device_info_util.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using base::Time;
using base::TimeDelta;

DeviceInfoSyncService::DeviceInfoSyncService(
    LocalDeviceInfoProvider* local_device_info_provider)
    : local_device_info_provider_(local_device_info_provider) {
  DCHECK(local_device_info_provider);
}

DeviceInfoSyncService::~DeviceInfoSyncService() {}

SyncMergeResult DeviceInfoSyncService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    std::unique_ptr<SyncChangeProcessor> sync_processor,
    std::unique_ptr<SyncErrorFactory> error_handler) {
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());
  DCHECK_EQ(type, DEVICE_INFO);

  DCHECK(!IsSyncing());

  sync_processor_ = std::move(sync_processor);
  error_handler_ = std::move(error_handler);

  // Initialization should be completed before this type is enabled
  // and local device info must be available.
  const DeviceInfo* local_device_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  DCHECK(local_device_info != nullptr);

  // Indicates whether a local device has been added or updated.
  // |change_type| defaults to ADD and might be changed to
  // UPDATE to INVALID down below if the initial data contains
  // data matching the local device ID.
  SyncChange::SyncChangeType change_type = SyncChange::ACTION_ADD;
  TimeDelta pulse_delay;
  size_t num_items_new = 0;
  size_t num_items_updated = 0;

  // Iterate over all initial sync data and copy it to the cache.
  for (SyncDataList::const_iterator iter = initial_sync_data.begin();
       iter != initial_sync_data.end(); ++iter) {
    DCHECK_EQ(DEVICE_INFO, iter->GetDataType());

    const std::string& id = iter->GetSpecifics().device_info().cache_guid();

    if (id == local_device_info->guid()) {
      // |initial_sync_data| contains data matching the local device.
      std::unique_ptr<DeviceInfo> synced_local_device_info =
          base::WrapUnique(CreateDeviceInfo(*iter));

      pulse_delay = DeviceInfoUtil::CalculatePulseDelay(
          GetLastUpdateTime(*iter), Time::Now());
      // Store the synced device info for the local device only if
      // it is the same as the local info. Otherwise store the local
      // device info and issue a change further below after finishing
      // processing the |initial_sync_data|.
      if (synced_local_device_info->Equals(*local_device_info) &&
          !pulse_delay.is_zero()) {
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

  SyncMergeResult result(type);

  // If the SyncData for the local device is new or different then send it
  // immediately, otherwise wait until the pulse interval has elapsed from the
  // previous update. Regardless of the branch here we setup a timer loop with
  // SendLocalData such that we continue pulsing every interval.
  if (change_type == SyncChange::ACTION_INVALID) {
    pulse_timer_.Start(
        FROM_HERE, pulse_delay,
        base::Bind(&DeviceInfoSyncService::SendLocalData,
                   base::Unretained(this), SyncChange::ACTION_UPDATE));
  } else {
    SendLocalData(change_type);
  }

  result.set_num_items_before_association(1);
  result.set_num_items_after_association(all_data_.size());
  result.set_num_items_added(num_items_new);
  result.set_num_items_modified(num_items_updated);
  result.set_num_items_deleted(0);

  NotifyObservers();

  return result;
}

bool DeviceInfoSyncService::IsSyncing() const {
  return !all_data_.empty();
}

void DeviceInfoSyncService::StopSyncing(ModelType type) {
  bool was_syncing = IsSyncing();

  pulse_timer_.Stop();
  all_data_.clear();
  sync_processor_.reset();
  error_handler_.reset();

  if (was_syncing) {
    NotifyObservers();
  }
}

SyncDataList DeviceInfoSyncService::GetAllSyncData(ModelType type) const {
  SyncDataList list;

  for (SyncDataMap::const_iterator iter = all_data_.begin();
       iter != all_data_.end(); ++iter) {
    list.push_back(iter->second);
  }

  return list;
}

SyncError DeviceInfoSyncService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  SyncError error;

  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());
  const std::string& local_device_id =
      local_device_info_provider_->GetLocalDeviceInfo()->guid();

  bool has_changes = false;

  // Iterate over all chanages and merge entries.
  for (SyncChangeList::const_iterator iter = change_list.begin();
       iter != change_list.end(); ++iter) {
    const SyncData& sync_data = iter->sync_data();
    DCHECK_EQ(DEVICE_INFO, sync_data.GetDataType());

    const std::string& client_id =
        sync_data.GetSpecifics().device_info().cache_guid();
    // Ignore device info matching the local device.
    if (local_device_id == client_id) {
      DVLOG(1) << "Ignoring sync changes for the local DEVICE_INFO";
      continue;
    }

    if (iter->change_type() == SyncChange::ACTION_DELETE) {
      has_changes = true;
      DeleteSyncData(client_id);
    } else if (iter->change_type() == SyncChange::ACTION_UPDATE ||
               iter->change_type() == SyncChange::ACTION_ADD) {
      has_changes = true;
      StoreSyncData(client_id, sync_data);
    } else {
      error.Reset(FROM_HERE, "Invalid action received.", DEVICE_INFO);
    }
  }

  if (has_changes) {
    NotifyObservers();
  }

  return error;
}

std::unique_ptr<DeviceInfo> DeviceInfoSyncService::GetDeviceInfo(
    const std::string& client_id) const {
  SyncDataMap::const_iterator iter = all_data_.find(client_id);
  if (iter == all_data_.end()) {
    return std::unique_ptr<DeviceInfo>();
  }

  return base::WrapUnique(CreateDeviceInfo(iter->second));
}

std::vector<std::unique_ptr<DeviceInfo>>
DeviceInfoSyncService::GetAllDeviceInfo() const {
  std::vector<std::unique_ptr<DeviceInfo>> list;

  for (SyncDataMap::const_iterator iter = all_data_.begin();
       iter != all_data_.end(); ++iter) {
    list.push_back(base::WrapUnique(CreateDeviceInfo(iter->second)));
  }

  return list;
}

void DeviceInfoSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceInfoSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

int DeviceInfoSyncService::CountActiveDevices() const {
  return CountActiveDevices(Time::Now());
}

void DeviceInfoSyncService::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnDeviceInfoChange();
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
  specifics.set_last_updated_timestamp(TimeToProtoTime(Time::Now()));

  return CreateLocalData(entity);
}

SyncData DeviceInfoSyncService::CreateLocalData(
    const sync_pb::EntitySpecifics& entity) {
  const sync_pb::DeviceInfoSpecifics& specifics = entity.device_info();
  return SyncData::CreateLocalData(DeviceInfoUtil::SpecificsToTag(specifics),
                                   specifics.client_name(), entity);
}

DeviceInfo* DeviceInfoSyncService::CreateDeviceInfo(const SyncData& sync_data) {
  const sync_pb::DeviceInfoSpecifics& specifics =
      sync_data.GetSpecifics().device_info();

  return new DeviceInfo(specifics.cache_guid(), specifics.client_name(),
                        specifics.chrome_version(), specifics.sync_user_agent(),
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

void DeviceInfoSyncService::SendLocalData(
    const SyncChange::SyncChangeType change_type) {
  DCHECK_NE(change_type, SyncChange::ACTION_INVALID);
  DCHECK(sync_processor_);

  const DeviceInfo* device_info =
      local_device_info_provider_->GetLocalDeviceInfo();

  // While the |pulse_timer_| is shutdown eventually in StopSyncing(), our
  // device info provider is ripped out from underneath us before that happens,
  // and we need to guard against this.
  if (device_info != nullptr) {
    const SyncData& data = CreateLocalData(device_info);
    StoreSyncData(device_info->guid(), data);

    SyncChangeList change_list;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  }

  pulse_timer_.Start(
      FROM_HERE, DeviceInfoUtil::kPulseInterval,
      base::Bind(&DeviceInfoSyncService::SendLocalData, base::Unretained(this),
                 SyncChange::ACTION_UPDATE));
}

int DeviceInfoSyncService::CountActiveDevices(const Time now) const {
  return std::count_if(all_data_.begin(), all_data_.end(),
                       [now](SyncDataMap::const_reference pair) {
                         return DeviceInfoUtil::IsActive(
                             GetLastUpdateTime(pair.second), now);
                       });
}

// static
Time DeviceInfoSyncService::GetLastUpdateTime(const SyncData& device_info) {
  if (device_info.GetSpecifics().device_info().has_last_updated_timestamp()) {
    return ProtoTimeToTime(
        device_info.GetSpecifics().device_info().last_updated_timestamp());
  } else if (!device_info.IsLocal()) {
    // If there is no |last_updated_timestamp| present, fallback to mod time.
    return SyncDataRemote(device_info).GetModifiedTime();
  } else {
    // We shouldn't reach this point for remote data, so this means we're likely
    // looking at the local device info. Using a long ago time is perfect, since
    // the desired behavior is to update/pulse our data as soon as possible.
    return Time();
  }
}

}  // namespace syncer
