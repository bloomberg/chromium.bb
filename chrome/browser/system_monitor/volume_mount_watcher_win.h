// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/system_monitor/system_monitor.h"

namespace chrome {

// This class watches the volume mount points and sends notifications to
// base::SystemMonitor about the device attach/detach events. This is a
// singleton class instantiated by RemovableDeviceNotificationsWindowWin.
class VolumeMountWatcherWin
    : public base::RefCountedThreadSafe<VolumeMountWatcherWin> {
 public:
  VolumeMountWatcherWin();

  // Returns the volume file path of the drive specified by the |drive_number|.
  // |drive_number| inputs of 0 - 25 are valid. Returns an empty file path if
  // the |drive_number| is invalid.
  static FilePath DriveNumberToFilePath(int drive_number);

  // Must be called after the file thread is created.
  void Init();

  // Gets the information about the device mounted at |device_path|. On success,
  // returns true and fills in |location|, |unique_id|, |name|, and |removable|.
  virtual bool GetDeviceInfo(const FilePath& device_path,
                             string16* location,
                             std::string* unique_id,
                             string16* name,
                             bool* removable);

  // Processes DEV_BROADCAST_VOLUME messages and triggers a SystemMonitor
  // notification if appropriate.
  void OnWindowMessage(UINT event_type, LPARAM data);

 protected:
  // VolumeMountWatcherWin is ref-counted.
  virtual ~VolumeMountWatcherWin();

  // Returns a vector of all the removable mass storage devices that are
  // connected.
  virtual std::vector<FilePath> GetAttachedDevices();

 private:
  friend class base::RefCountedThreadSafe<VolumeMountWatcherWin>;

  struct MountPointInfo {
    std::string device_id;
  };

  // Key: Mass storage device mount point.
  // Value: Mass storage device metadata.
  typedef std::map<string16, MountPointInfo> MountPointDeviceMetadataMap;

  // Adds a new mass storage device specified by |device_path|.
  void AddNewDevice(const FilePath& device_path);

  // Enumerate and add existing mass storage devices on file thread.
  void AddExistingDevicesOnFileThread();

  // Identifies the device type and handles the device attach event.
  void CheckDeviceTypeOnFileThread(const std::string& unique_id,
                                   const string16& device_name,
                                   const FilePath& device_path);

  // Handles mass storage device attach event on UI thread.
  void HandleDeviceAttachEventOnUIThread(const std::string& device_id,
                                         const string16& device_name,
                                         const FilePath& device_path);

  // Handles mass storage device detach event on UI thread.
  void HandleDeviceDetachEventOnUIThread(const string16& device_location);

  // A map from device mount point to device metadata. Only accessed on the UI
  // thread.
  MountPointDeviceMetadataMap device_metadata_;

  DISALLOW_COPY_AND_ASSIGN(VolumeMountWatcherWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_VOLUME_MOUNT_WATCHER_WIN_H_
