// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_TRACKER_H_
#define CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_TRACKER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/sync/glue/device_info.h"

namespace browser_sync {

// Interface for tracking synced DeviceInfo.
class DeviceInfoTracker {
 public:
  virtual ~DeviceInfoTracker() {}

  // Observer class for listening to device info changes.
  class Observer {
   public:
    virtual void OnDeviceInfoChange() = 0;
  };

  // Gets DeviceInfo the synced device with specified client ID.
  // Returns an empty scoped_ptr if device with the given |client_id| hasn't
  // been synced.
  virtual scoped_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const = 0;
  // Gets DeviceInfo for all synced devices (including the local one).
  virtual ScopedVector<DeviceInfo> GetAllDeviceInfo() const = 0;
  // Registers an observer to be called on syncing any updated DeviceInfo.
  virtual void AddObserver(Observer* observer) = 0;
  // Unregisters an observer.
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DEVICE_INFO_TRACKER_H_
