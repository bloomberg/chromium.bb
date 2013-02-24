// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome::MediaStorageUtil provides information about storage devices attached
// to the computer.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_MEDIA_STORAGE_UTIL_H_
#define CHROME_BROWSER_STORAGE_MONITOR_MEDIA_STORAGE_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace chrome {

class MediaStorageUtil {
 public:
  enum Type {
    // A removable mass storage device with a DCIM directory.
    REMOVABLE_MASS_STORAGE_WITH_DCIM,
    // A removable mass storage device without a DCIM directory.
    REMOVABLE_MASS_STORAGE_NO_DCIM,
    // A fixed mass storage device.
    FIXED_MASS_STORAGE,
    // A MTP or PTP device.
    MTP_OR_PTP,
    // A Mac ImageCapture device.
    MAC_IMAGE_CAPTURE,
  };

  typedef std::set<std::string /*device id*/> DeviceIdSet;
  typedef base::Callback<void(bool)> BoolCallback;

  // Returns a device id given properties of the device. A prefix dependent on
  // |type| is added so |unique_id| need only be unique within the given type.
  // Returns an empty string if an invalid type is passed in.
  static std::string MakeDeviceId(Type type, const std::string& unique_id);

  // Extracts the device |type| and |unique_id| from |device_id|. Returns false
  // if the device_id isn't properly formatted.
  static bool CrackDeviceId(const std::string& device_id,
                            Type* type, std::string* unique_id);

  // Looks inside |device_id| to determine if it is a media device
  // (type is REMOVABLE_MASS_STORAGE_WITH_DCIM or MTP_OR_PTP).
  static bool IsMediaDevice(const std::string& device_id);

  // Looks inside |device_id| to determine if it is a media device
  // (type isn't FIXED_MASS_STORAGE).
  static bool IsRemovableDevice(const std::string& device_id);

  // Looks inside |device_id| to determine if it is a mass storage device
  // (type isn't MTP_OR_PTP).
  static bool IsMassStorageDevice(const std::string& device_id);

  // Returns true if we will be able to create a filesystem for this device.
  static bool CanCreateFileSystem(const std::string& device_id,
                                  const base::FilePath& path);

  // Determines if the device is attached to the computer.
  static void IsDeviceAttached(const std::string& device_id,
                               const BoolCallback& callback);

  // Removes disconnected devices from |devices| and then calls |done|.
  static void FilterAttachedDevices(DeviceIdSet* devices,
                                    const base::Closure& done);

  // Given |path|, fill in |device_id|, |device_name|, and |relative_path|
  // (from the root of the device) if they are not NULL.
  // TODO(gbillock): This needs to have an outparam of type
  // RemovableStorageNotifications::StorageInfo.
  static bool GetDeviceInfoFromPath(const base::FilePath& path,
                                    std::string* device_id,
                                    string16* device_name,
                                    base::FilePath* relative_path);

  // Get a base::FilePath for the given |device_id|.  If the device isn't a mass
  // storage type, the base::FilePath will be empty.  This does not check that
  // the device is connected.
  static base::FilePath FindDevicePathById(const std::string& device_id);

  // Record device information histogram for the given |device_uuid| and
  // |device_name|. |mass_storage| indicates whether the current device is a
  // mass storage device, as defined by IsMassStorageDevice().
  static void RecordDeviceInfoHistogram(bool mass_storage,
                                        const std::string& device_uuid,
                                        const string16& device_name);

 protected:
  typedef bool (*GetDeviceInfoFromPathFunction)(const base::FilePath& path,
                                                std::string* device_id,
                                                string16* device_name,
                                                base::FilePath* relative_path);

  // Set the implementation of GetDeviceInfoFromPath for testing.
  static void SetGetDeviceInfoFromPathFunctionForTesting(
      GetDeviceInfoFromPathFunction function);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaStorageUtil);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_MEDIA_STORAGE_UTIL_H_
