// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_MAP_SERVICE_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_MAP_SERVICE_H_

#include <map>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_checker.h"

class MTPDeviceAsyncDelegate;

// This class provides media transfer protocol (MTP) device delegate to
// complete media file system operations.
// Lives on the IO thread in production.
// TODO(gbillock): Make this class owned by the MediaFileSystemRegistry.
class MTPDeviceMapService {
 public:
  static MTPDeviceMapService* GetInstance();

  // Gets the media device delegate associated with |filesystem_id|.
  // Return NULL if the |filesystem_id| is no longer valid (e.g. because the
  // corresponding device is detached, etc).
  // Called on the IO thread.
  MTPDeviceAsyncDelegate* GetMTPDeviceAsyncDelegate(
      const std::string& filesystem_id);

  // Register that an MTP filesystem is in use for the given |device_location|.
  void RegisterMTPFileSystem(
    const base::FilePath::StringType& device_location,
    const std::string& fsid);

  // Removes the MTP entry associated with the given
  // |device_location|. Signals the MTPDeviceMapService to destroy the
  // delegate if there are no more uses of it.
  void RevokeMTPFileSystem(const std::string& fsid);

 private:
  friend struct base::DefaultLazyInstanceTraits<MTPDeviceMapService>;

  // Adds the MTP device delegate to the map service. |device_location|
  // specifies the mount location of the MTP device.
  // Called on the IO thread.
  void AddAsyncDelegate(const base::FilePath::StringType& device_location,
                        MTPDeviceAsyncDelegate* delegate);

  // Removes the MTP device delegate from the map service. |device_location|
  // specifies the mount location of the MTP device.
  // Called on the IO thread.
  void RemoveAsyncDelegate(const base::FilePath::StringType& device_location);

  // Mapping of device_location and MTPDeviceAsyncDelegate* object. It is safe
  // to store and access the raw pointer. This class operates on the IO thread.
  typedef std::map<base::FilePath::StringType, MTPDeviceAsyncDelegate*>
      AsyncDelegateMap;

  // Map a filesystem id (fsid) to an MTP device location.
  typedef std::map<std::string, base::FilePath::StringType>
      MTPDeviceFileSystemMap;

  // Map a MTP or PTP device location to a count of current uses of that
  // location.
  typedef std::map<const base::FilePath::StringType, int>
      MTPDeviceUsageMap;


  // Get access to this class using GetInstance() method.
  MTPDeviceMapService();
  ~MTPDeviceMapService();

  // Map of attached mtp device async delegates.
  AsyncDelegateMap async_delegate_map_;

  MTPDeviceFileSystemMap mtp_device_map_;

  MTPDeviceUsageMap mtp_device_usage_map_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceMapService);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MTP_DEVICE_MAP_SERVICE_H_
