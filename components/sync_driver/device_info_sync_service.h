// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SYNC_SERVICE_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "components/sync_driver/device_info_tracker.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"

namespace sync_driver {

class LocalDeviceInfoProvider;

// SyncableService implementation for DEVICE_INFO model type.
class DeviceInfoSyncService : public syncer::SyncableService,
                              public DeviceInfoTracker {
 public:
  explicit DeviceInfoSyncService(
      LocalDeviceInfoProvider* local_device_info_provider);
  ~DeviceInfoSyncService() override;

  // syncer::SyncableService implementation.
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // DeviceInfoTracker implementation.
  bool IsSyncing() const override;
  scoped_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  ScopedVector<DeviceInfo> GetAllDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Called to update local device backup time.
  void UpdateLocalDeviceBackupTime(base::Time backup_time);
  // Gets the most recently set local device backup time.
  base::Time GetLocalDeviceBackupTime() const;

 private:
  // Create SyncData from local DeviceInfo and |local_device_backup_time_|.
  syncer::SyncData CreateLocalData(const DeviceInfo* info);
  // Create SyncData from EntitySpecifics.
  static syncer::SyncData CreateLocalData(
      const sync_pb::EntitySpecifics& entity);

  // Allocate new DeviceInfo from SyncData.
  static DeviceInfo* CreateDeviceInfo(const syncer::SyncData& sync_data);
  // Store SyncData in the cache.
  void StoreSyncData(const std::string& client_id,
                     const syncer::SyncData& sync_data);
  // Delete SyncData from the cache.
  void DeleteSyncData(const std::string& client_id);
  // Notify all registered observers.
  void NotifyObservers();

  // Updates backup time in place in |sync_data| if it is different than
  // the one stored in |local_device_backup_time_|.
  // Returns true if backup time was updated.
  bool UpdateBackupTime(syncer::SyncData* sync_data);

  // |local_device_backup_time_| accessors.
  int64 local_device_backup_time() const { return local_device_backup_time_; }
  bool has_local_device_backup_time() const {
    return local_device_backup_time_ >= 0;
  }
  void set_local_device_backup_time(int64 value) {
    local_device_backup_time_ = value;
  }
  void clear_local_device_backup_time() { local_device_backup_time_ = -1; }

  // Local device last set backup time (in proto format).
  // -1 if the value hasn't been specified
  int64 local_device_backup_time_;

  // |local_device_info_provider_| isn't owned.
  const LocalDeviceInfoProvider* const local_device_info_provider_;

  // Receives ownership of |sync_processor_| and |error_handler_| in
  // MergeDataAndStartSyncing() and destroy them in StopSyncing().
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> error_handler_;

  // Cache of all syncable and local data.
  typedef std::map<std::string, syncer::SyncData> SyncDataMap;
  SyncDataMap all_data_;

  // Registered observers, not owned.
  base::ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncService);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SYNC_SERVICE_H_
