// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SYNC_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync_driver/device_info_tracker.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"

namespace sync_driver {

class LocalDeviceInfoProvider;

// The delay between periodic updates to the entry corresponding to this device.
extern const base::TimeDelta kDeviceInfoPulseInterval;

// The amount of time a device can go without an updates before we consider it
// stale/inactive, and start ignoring it for active device counts.
extern const base::TimeDelta kStaleDeviceInfoThreshold;

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
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  // DeviceInfoTracker implementation.
  bool IsSyncing() const override;
  std::unique_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  ScopedVector<DeviceInfo> GetAllDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  int CountActiveDevices() const override;

 private:
  friend class DeviceInfoSyncServiceTest;

  // Create SyncData from local DeviceInfo.
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

  // Find the timestamp for the last time this |device_info| was edited.
  static base::Time GetLastUpdateTime(const syncer::SyncData& device_info);

  // Finds the amount of time since this |device_info| was last edited. If this
  // |device_info| claims to have been edited in the future, the smallest age
  // this returns will be an age of zero and never negative.
  static base::TimeDelta GetLastUpdateAge(const syncer::SyncData& device_info,
                                          const base::Time now);

  // Determines the amount of time before we should pulse and update the entity
  // that corresponds to this device. This value is calculated by looking at
  // time |now|, the last updated timestamp in |device_info|, and using the
  // pulse interval. The smallest delay this will ever return will be the
  // instant delay and never negative.
  static base::TimeDelta CalculatePulseDelay(
      const syncer::SyncData& device_info,
      const base::Time now);

  // Sends a copy of the current device's state to the processor/sync.
  void SendLocalData(const syncer::SyncChange::SyncChangeType change_type);

  // Counts the number of active devices relative to |now|. The activeness of a
  // device depends on the amount of time since it was updated, which means
  // comparing it against the current time. |now| is passed into this method to
  // allow unit tests to control expected results.
  int CountActiveDevices(const base::Time now) const;

  // |local_device_info_provider_| isn't owned.
  const LocalDeviceInfoProvider* const local_device_info_provider_;

  // Receives ownership of |sync_processor_| and |error_handler_| in
  // MergeDataAndStartSyncing() and destroy them in StopSyncing().
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;

  // Cache of all syncable and local data.
  typedef std::map<std::string, syncer::SyncData> SyncDataMap;
  SyncDataMap all_data_;

  // Registered observers, not owned.
  base::ObserverList<Observer, true> observers_;

  // Used to update our local device info once every pulse interval.
  base::OneShotTimer pulse_timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncService);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DEVICE_INFO_SYNC_SERVICE_H_
