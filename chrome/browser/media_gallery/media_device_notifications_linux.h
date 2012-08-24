// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaDeviceNotificationsLinux listens for mount point changes and notifies
// the SystemMonitor about the addition and deletion of media devices.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_LINUX_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_LINUX_H_

#if defined(OS_CHROMEOS)
#error "Use the ChromeOS-specific implementation instead."
#endif

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"

class FilePath;

// Gets the media device information given a |device_path|. On success,
// returns true and fills in |name| and |id|.
typedef bool (*GetDeviceInfoFunc)(const std::string& device_path,
                                  std::string* id,
                                  string16* name);

namespace chrome {

class MediaDeviceNotificationsLinux
    : public base::RefCountedThreadSafe<MediaDeviceNotificationsLinux,
          content::BrowserThread::DeleteOnFileThread> {
 public:
  explicit MediaDeviceNotificationsLinux(const FilePath& path);

  // Must be called for MediaDeviceNotificationsLinux to work.
  void Init();

 protected:
  // Only for use in unit tests.
  MediaDeviceNotificationsLinux(const FilePath& path,
                                GetDeviceInfoFunc getDeviceInfo);

  // Avoids code deleting the object while there are references to it.
  // Aside from the base::RefCountedThreadSafe friend class, and derived
  // classes, any attempts to call this dtor will result in a compile-time
  // error.
  virtual ~MediaDeviceNotificationsLinux();

  virtual void OnFilePathChanged(const FilePath& path, bool error);

 private:
  friend class base::RefCountedThreadSafe<MediaDeviceNotificationsLinux>;
  friend class base::DeleteHelper<MediaDeviceNotificationsLinux>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::FILE>;

  // Structure to save mounted device information such as device path and unique
  // identifier.
  struct MountDeviceAndId {
    std::string mount_device;
    std::string device_id;
  };

  // Mapping of mount points to MountDeviceAndId.
  typedef std::map<std::string, MountDeviceAndId> MountMap;

  // (mount point, mount device)
  // Helper Map to get new entries from mtab file.
  typedef std::map<std::string, std::string> MountPointDeviceMap;

  // Do initialization on the File Thread.
  void InitOnFileThread();

  // Parses mtab file and find all changes.
  void UpdateMtab();

  // Reads mtab file entries into |mtab|.
  void ReadMtab(MountPointDeviceMap* mtab);

  // Checks and adds |mount_device| as media device given the |mount_point|.
  void CheckAndAddMediaDevice(const std::string& mount_device,
                              const std::string& mount_point);

  // Removes media device with a given device id.
  void RemoveOldDevice(const std::string& device_id);

  // Whether Init() has been called or not.
  bool initialized_;

  // Mtab file that lists the mount points.
  const FilePath mtab_path_;

  // Watcher for |mtab_path_|.
  base::files::FilePathWatcher file_watcher_;

  // Mapping of relevant mount points and their corresponding mount devices.
  // Keep in mind on Linux, a device can be mounted at multiple mount points,
  // and multiple devices can be mounted at a mount point.
  MountMap mount_info_map_;

  // Set of known file systems that we care about.
  std::set<std::string> known_file_systems_;

  // Function handler to get device information. This is useful to set a mock
  // handler for unit testing.
  GetDeviceInfoFunc get_device_info_func_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceNotificationsLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_LINUX_H_
