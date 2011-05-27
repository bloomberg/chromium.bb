// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
#pragma once

#include <string>
#include <map>

#include "base/memory/linked_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/cros/mount_library.h"

class Profile;

namespace chromeos {
class SystemNotification;
}

// Used to monitor disk mount changes and signal when new mounted usb device is
// found.
class ExtensionFileBrowserEventRouter
    : public chromeos::MountLibrary::Observer {
 public:
  static ExtensionFileBrowserEventRouter* GetInstance();

  // Starts/stops observing file system change events. Currently only
  // MountLibrary events are being observed.
  void ObserveFileSystemEvents(Profile* profile);
  void StopObservingFileSystemEvents();

  // MountLibrary::Observer overrides.
  virtual void DiskChanged(chromeos::MountLibraryEventType event,
                           const chromeos::MountLibrary::Disk* disk);
  virtual void DeviceChanged(chromeos::MountLibraryEventType event,
                             const std::string& device_path);

 private:
  friend struct DefaultSingletonTraits<ExtensionFileBrowserEventRouter>;
  typedef std::map<std::string, linked_ptr<chromeos::SystemNotification> >
      NotificationMap;
  typedef std::map<std::string, std::string> MountPointMap;

  ExtensionFileBrowserEventRouter();
  virtual ~ExtensionFileBrowserEventRouter();

  // USB mount event handlers.
  void OnDiskAdded(const chromeos::MountLibrary::Disk* disk);
  void OnDiskRemoved(const chromeos::MountLibrary::Disk* disk);
  void OnDiskChanged(const chromeos::MountLibrary::Disk* disk);
  void OnDeviceAdded(const std::string& device_path);
  void OnDeviceRemoved(const std::string& device_path);
  void OnDeviceScanned(const std::string& device_path);

  // Finds first notifications corresponding to the same device. Ensures that
  // we don't pop up multiple notifications for the same device.
  NotificationMap::iterator FindNotificationForPath(const std::string& path);

  // Sends filesystem changed extension message to all renderers.
  void DispatchEvent(const chromeos::MountLibrary::Disk* disk, bool added);

  void RemoveBrowserFromVector(const std::string& path);

  // Used to create a window of a standard size, and add it to a list
  // of tracked browser windows in case that device goes away.
  void OpenFileBrowse(const std::string& url,
                      const std::string& device_path,
                      bool small);

  // Show/hide desktop notifications.
  void ShowDeviceNotification(const std::string& system_path,
                              int icon_resource_id,
                              const string16& message);
  void HideDeviceNotification(const std::string& system_path);

  MountPointMap mounted_devices_;
  NotificationMap notifications_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFileBrowserEventRouter);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
