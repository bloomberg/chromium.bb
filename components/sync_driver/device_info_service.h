// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SERVICE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/sync_driver/device_info_tracker.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "sync/api/model_type_service.h"
#include "sync/internal_api/public/simple_metadata_change_list.h"

namespace syncer {
class SyncError;
}  // namespace syncer

namespace syncer_v2 {
class ModelTypeChangeProcessor;
}  // namespace syncer_v2

namespace sync_pb {
class DeviceInfoSpecifics;
}  // namespace sync_pb

namespace sync_driver_v2 {

// USS service implementation for DEVICE_INFO model type. Handles storage of
// device info and associated sync metadata, applying/merging foreign changes,
// and allows public read access.
class DeviceInfoService : public syncer_v2::ModelTypeService,
                          public sync_driver::DeviceInfoTracker {
 public:
  explicit DeviceInfoService(
      sync_driver::LocalDeviceInfoProvider* local_device_info_provider);
  ~DeviceInfoService() override;

  // ModelTypeService implementation.
  scoped_ptr<syncer_v2::MetadataChangeList> CreateMetadataChangeList() override;
  syncer::SyncError MergeSyncData(
      scoped_ptr<syncer_v2::MetadataChangeList> metadata_change_list,
      syncer_v2::EntityDataList entity_data_list) override;
  syncer::SyncError ApplySyncChanges(
      scoped_ptr<syncer_v2::MetadataChangeList> metadata_change_list,
      syncer_v2::EntityChangeList entity_changes) override;
  void LoadMetadata(MetadataCallback callback) override;
  void GetData(ClientTagList client_tags, DataCallback callback) override;
  void GetAllData(DataCallback callback) override;
  std::string GetClientTag(const syncer_v2::EntityData& entity_data) override;

  // DeviceInfoTracker implementation.
  bool IsSyncing() const override;
  scoped_ptr<sync_driver::DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  ScopedVector<sync_driver::DeviceInfo> GetAllDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Called to update local device backup time.
  void UpdateLocalDeviceBackupTime(base::Time backup_time);
  // Gets the most recently set local device backup time.
  base::Time GetLocalDeviceBackupTime() const;

 private:
  friend class DeviceInfoServiceTest;

  scoped_ptr<sync_pb::DeviceInfoSpecifics> CreateLocalSpecifics();
  static scoped_ptr<sync_pb::DeviceInfoSpecifics> CreateSpecifics(
      const sync_driver::DeviceInfo& info);

  // Allocate new DeviceInfo from SyncData.
  static scoped_ptr<sync_driver::DeviceInfo> CreateDeviceInfo(
      const sync_pb::DeviceInfoSpecifics& specifics);

  // Store SyncData in the cache.
  void StoreSpecifics(scoped_ptr<sync_pb::DeviceInfoSpecifics> specifics);
  // Delete SyncData from the cache.
  void DeleteSpecifics(const std::string& client_id);

  // Notify all registered observers.
  void NotifyObservers();

  void OnProviderInitialized();

  // |local_device_backup_time_| accessors.
  int64_t local_device_backup_time() const { return local_device_backup_time_; }
  bool has_local_device_backup_time() const {
    return local_device_backup_time_ >= 0;
  }
  void set_local_device_backup_time(int64_t value) {
    local_device_backup_time_ = value;
  }
  void clear_local_device_backup_time() { local_device_backup_time_ = -1; }

  // TODO(skym): Remove once we remove local provider.
  // Local device last set backup time (in proto format).
  // -1 if the value hasn't been specified
  int64_t local_device_backup_time_;

  // |local_device_info_provider_| isn't owned.
  const sync_driver::LocalDeviceInfoProvider* const local_device_info_provider_;

  // TODO(skym): Switch to use client tag hash instead of cache guid as key.
  // Cache of all syncable and local data, stored by device cache guid.
  using ClientIdToSpecifics =
      std::map<std::string, scoped_ptr<sync_pb::DeviceInfoSpecifics>>;
  ClientIdToSpecifics all_data_;

  // Registered observers, not owned.
  base::ObserverList<Observer, true> observers_;

  scoped_ptr<sync_driver::LocalDeviceInfoProvider::Subscription> subscription_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoService);
};

}  // namespace sync_driver_v2

#endif  // COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SERVICE_H_
