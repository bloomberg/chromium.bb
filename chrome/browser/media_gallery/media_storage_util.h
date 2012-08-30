// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome::MediaStorageUtil provides information about storage devices attached
// to the computer.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_STORAGE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_STORAGE_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/file_path.h"

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
  };

  typedef base::Callback<void(bool)> BoolCallback;
  typedef base::Callback<void(FilePath)> FilePathCallback;
  typedef base::Callback<void(std::string /*device id*/,
                              FilePath /*path relative to device root*/,
                              string16 /*display name*/)> DeviceInfoCallback;

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

  // Determines if the device is attached to the computer.
  static void IsDeviceAttached(const std::string& device_id,
                               const BoolCallback& callback);

  // Given |path|, get the device_id, relative path from the root of the
  // device, and the device name.
  static void GetDeviceInfoFromPath(const FilePath& path,
                                    const DeviceInfoCallback& callback);

  // Get a FilePath for the given |device_id|.  If the device isn't connected
  // or isn't a mass storage type, the FilePath will be empty.
  static void FindDevicePathById(const std::string& device_id,
                                 const FilePathCallback& callback);

 private:
  // All methods are static, this class should not be instantiated.
  MediaStorageUtil();

  // A platform specific helper.
  static void FindUSBDeviceById(const std::string& device_id,
                                const FilePathCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(MediaStorageUtil);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_STORAGE_UTIL_H_
