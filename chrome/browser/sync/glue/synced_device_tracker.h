// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_DEVICE_TRACKER_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_DEVICE_TRACKER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "components/sync_driver/change_processor.h"

namespace syncer {
struct UserShare;
}

namespace browser_sync {

class DeviceInfo;

class SyncedDeviceTracker : public sync_driver::ChangeProcessor {
 public:
  SyncedDeviceTracker(syncer::UserShare* user_share,
                      const std::string& cache_guid);
  virtual ~SyncedDeviceTracker();

  // Observer class for listening to device info changes.
  class Observer {
   public:
    virtual void OnDeviceInfoChange() = 0;
  };

  // ChangeProcessor methods
  virtual void StartImpl() OVERRIDE;
  virtual void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64 model_version,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE;
  virtual void CommitChangesFromSyncModel() OVERRIDE;

  // Methods for accessing and updating the device_info list.
  // These are virtual for testing.
  virtual scoped_ptr<DeviceInfo> ReadLocalDeviceInfo(
      const syncer::BaseTransaction &trans) const;
  virtual scoped_ptr<DeviceInfo> ReadLocalDeviceInfo() const;
  virtual void InitLocalDeviceInfo(const std::string& signin_scoped_device_id,
                                   const base::Closure& callback);
  virtual scoped_ptr<DeviceInfo> ReadDeviceInfo(
      const std::string& client_id) const;
  virtual void GetAllSyncedDeviceInfo(
      ScopedVector<DeviceInfo>* device_info) const;

  virtual std::string cache_guid() const;

  // Can be called on any thread. Will be notified back on the same thread
  // they were called on. Observer will be called on every device info
  // change.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Update |backup_timestamp| in local device info specifics to |backup_time|
  // if different.
  void UpdateLocalDeviceBackupTime(base::Time backup_time);

  // Return time derived from |backup_timestamp| in local device info specifics.
  base::Time GetLocalDeviceBackupTime() const;

 private:
  friend class SyncedDeviceTrackerTest;

  void InitLocalDeviceInfoContinuation(const base::Closure& callback,
                                       const DeviceInfo& local_info);

  // Helper to write specifics into our node.  Also useful for testing.
  void WriteLocalDeviceInfo(const DeviceInfo& info);

  // Helper to write arbitrary device info. Useful for writing local device
  // info and also used by test cases to write arbitrary device infos.
  void WriteDeviceInfo(const DeviceInfo& info, const std::string& tag);

  syncer::UserShare* user_share_;
  const std::string cache_guid_;
  const std::string local_device_info_tag_;

  // The |ObserverList| has to be thread safe as the changes happen
  // on sync thread and the observers could be on any thread.
  typedef ObserverListThreadSafe<Observer> ObserverList;
  scoped_refptr<ObserverList> observers_;

  base::WeakPtrFactory<SyncedDeviceTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncedDeviceTracker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_DEVICE_TRACKER_H_
