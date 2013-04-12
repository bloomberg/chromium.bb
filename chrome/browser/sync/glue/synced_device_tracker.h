// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_DEVICE_TRACKER_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_DEVICE_TRACKER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/change_processor.h"

namespace syncer {
struct UserShare;
}

namespace browser_sync {

class DeviceInfo;

class SyncedDeviceTracker : public ChangeProcessor {
 public:
  SyncedDeviceTracker(syncer::UserShare* user_share,
                      const std::string& cache_guid);
  virtual ~SyncedDeviceTracker();

  // ChangeProcessor methods
  virtual void StartImpl(Profile* profile) OVERRIDE;
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
  virtual void InitLocalDeviceInfo(const base::Closure& callback);
  virtual scoped_ptr<DeviceInfo> ReadDeviceInfo(
      const std::string& client_id) const;

 private:
  friend class SyncedDeviceTrackerTest;

  void InitLocalDeviceInfoContinuation(const base::Closure& callback,
                                       const DeviceInfo& local_info);

  // Helper to write specifics into our node.  Also useful for testing.
  void WriteLocalDeviceInfo(const DeviceInfo& info);

  base::WeakPtrFactory<SyncedDeviceTracker> weak_factory_;

  syncer::UserShare* user_share_;
  const std::string cache_guid_;
  const std::string local_device_info_tag_;

  DISALLOW_COPY_AND_ASSIGN(SyncedDeviceTracker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_DEVICE_TRACKER_H_
