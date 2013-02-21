// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
#define CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_

#include "base/callback.h"
#include "base/file_path.h"
#include "base/observer_list_threadsafe.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"

class ChromeBrowserMainPartsLinux;
class ChromeBrowserMainPartsMac;
class MediaGalleriesPrivateApiTest;
class MediaGalleriesPrivateEjectApiTest;

namespace chrome {

class MediaFileSystemRegistryTest;
class RemovableStorageObserver;
class TransientDeviceIds;

// Base class for platform-specific instances watching for removable storage
// attachments/detachments.
class RemovableStorageNotifications {
 public:
  struct StorageInfo {
    StorageInfo();
    StorageInfo(const std::string& id,
                const string16& device_name,
                const base::FilePath::StringType& device_location);

    // Unique device id - persists between device attachments.
    std::string device_id;

    // Human readable removable storage device name.
    string16 name;

    // Current attached removable storage device location.
    base::FilePath::StringType location;
  };

  // This interface is provided to generators of storage notifications.
  class Receiver {
   public:
    virtual ~Receiver();

    virtual void ProcessAttach(const std::string& id,
                               const string16& name,
                               const base::FilePath::StringType& location) = 0;
    virtual void ProcessDetach(const std::string& id) = 0;
  };

  // Status codes for the result of an EjectDevice() call.
  enum EjectStatus {
    EJECT_OK,
    EJECT_IN_USE,
    EJECT_NO_SUCH_DEVICE,
    EJECT_FAILURE
  };

  // Returns a pointer to an object owned by the BrowserMainParts, with lifetime
  // somewhat shorter than a process Singleton.
  static RemovableStorageNotifications* GetInstance();

  // Finds the device that contains |path| and populates |device_info|.
  // Should be able to handle any path on the local system, not just removable
  // storage. Returns false if unable to find the device.
  virtual bool GetDeviceInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const = 0;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero.
  virtual uint64 GetStorageSize(
      const base::FilePath::StringType& location) const = 0;

#if defined(OS_WIN)
  // Gets the MTP device storage information specified by |storage_device_id|.
  // On success, returns true and fills in |device_location| with device
  // interface details and |storage_object_id| with the string ID that
  // uniquely identifies the object on the device. This ID need not be
  // persistent across sessions.
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const = 0;
#endif

  // Returns information for attached removable storage.
  std::vector<StorageInfo> GetAttachedStorage() const;

  void AddObserver(RemovableStorageObserver* obs);
  void RemoveObserver(RemovableStorageObserver* obs);

  uint64 GetTransientIdForDeviceId(const std::string& device_id);
  std::string GetDeviceIdForTransientId(uint64 transient_id) const;

  virtual void EjectDevice(
      const std::string& device_id,
      base::Callback<void(EjectStatus)> callback);

 protected:
  RemovableStorageNotifications();
  virtual ~RemovableStorageNotifications();

  // Removes the existing singleton for testing.
  // (So that a new one can be created.)
  static void RemoveSingletonForTesting();

  // TODO(gbillock): Clean up ownerships and get rid of these friends.
  friend class ::ChromeBrowserMainPartsLinux;
  friend class ::ChromeBrowserMainPartsMac;
  friend class ::MediaGalleriesPrivateApiTest;
  friend class ::MediaGalleriesPrivateEjectApiTest;
  friend class MediaFileSystemRegistryTest;
  friend class TestRemovableStorageNotifications;

  virtual Receiver* receiver() const;

 private:
  class ReceiverImpl;
  friend class ReceiverImpl;

  typedef std::map<std::string, StorageInfo> RemovableStorageMap;

  void ProcessAttach(const StorageInfo& storage);
  void ProcessDetach(const std::string& id);

  scoped_ptr<Receiver> receiver_;

  scoped_refptr<ObserverListThreadSafe<RemovableStorageObserver> >
      observer_list_;

  // For manipulating removable_storage_map_ structure.
  mutable base::Lock storage_lock_;

  // Map of all the attached removable storage devices.
  RemovableStorageMap storage_map_;

  scoped_ptr<TransientDeviceIds> transient_device_ids_;
};

} // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
