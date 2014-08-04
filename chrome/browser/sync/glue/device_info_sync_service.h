// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_SYNC_SERVICE_H_

#include "base/observer_list.h"
#include "chrome/browser/sync/glue/device_info_tracker.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"

namespace browser_sync {

class LocalDeviceInfoProvider;

// SyncableService implementation for DEVICE_INFO model type.
class DeviceInfoSyncService : public syncer::SyncableService,
                              public DeviceInfoTracker {
 public:
  explicit DeviceInfoSyncService(
      LocalDeviceInfoProvider* local_device_info_provider);
  virtual ~DeviceInfoSyncService();

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // DeviceInfoTracker implementation.
  virtual scoped_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const OVERRIDE;
  virtual ScopedVector<DeviceInfo> GetAllDeviceInfo() const OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

 private:
  // Create SyncData from local DeviceInfo.
  static syncer::SyncData CreateLocalData(const DeviceInfo* info);
  // Allocate new DeviceInfo from SyncData.
  static DeviceInfo* CreateDeviceInfo(const syncer::SyncData sync_data);
  // Store SyncData in the cache.
  void StoreSyncData(const std::string& client_id,
                     const syncer::SyncData& sync_data);
  // Delete SyncData from the cache.
  void DeleteSyncData(const std::string& client_id);
  // Notify all registered observers.
  void NotifyObservers();

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
  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncService);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_SYNC_SERVICE_H_
