// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
#pragma once

#include <string>
#include <map>
#include <vector>

#include "chrome/browser/chromeos/cros/mount_library.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

class Browser;
template <typename T> struct DefaultSingletonTraits;
class Profile;

namespace chromeos { // NOLINT

// Used to monitor mount changes and popup a new window when
// a new mounted usb device is found.
class USBMountObserver : public MountLibrary::Observer,
                         public NotificationObserver {
 public:
  struct BrowserWithPath {
    Browser* browser;
    std::string device_path;
    std::string mount_path;
  };

  static USBMountObserver* GetInstance();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // MountLibrary::Observer overrides.
  virtual void DiskChanged(MountLibraryEventType event,
                           const MountLibrary::Disk* disk);
  virtual void DeviceChanged(MountLibraryEventType event,
                             const std::string& device_path);

 private:
  friend struct DefaultSingletonTraits<USBMountObserver>;
  typedef std::vector<BrowserWithPath>::iterator BrowserIterator;
  typedef std::map<std::string, std::string> MountPointMap;

  USBMountObserver() {}
  virtual ~USBMountObserver() {}

  // USB mount event handlers.
  void OnDiskAdded(const MountLibrary::Disk* disk);
  void OnDiskRemoved(const MountLibrary::Disk* disk);
  void OnDiskChanged(const MountLibrary::Disk* disk);
  void OnDeviceAdded(const std::string& device_path);
  void OnDeviceRemoved(const std::string& device_path);
  void OnDeviceScanned(const std::string& device_path);

  BrowserIterator FindBrowserForPath(const std::string& path);

  // Sends filesystem changed extension message to all renderers.
  void FireFileSystemChanged(const std::string& web_path);

  void RemoveBrowserFromVector(const std::string& path);

  // Used to create a window of a standard size, and add it to a list
  // of tracked browser windows in case that device goes away.
  void OpenFileBrowse(const std::string& url,
                      const std::string& device_path,
                      bool small);

  std::vector<BrowserWithPath> browsers_;
  NotificationRegistrar registrar_;
  MountPointMap mounted_devices_;

  DISALLOW_COPY_AND_ASSIGN(USBMountObserver);
};

} // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
