// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
#pragma once

#include <string>
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
class USBMountObserver : public chromeos::MountLibrary::Observer,
                         public NotificationObserver {
 public:
  struct BrowserWithPath {
    Browser* browser;
    std::string device_path;
    std::string mount_path;
  };

  static USBMountObserver* GetInstance();

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  void MountChanged(chromeos::MountLibrary* obj,
                    chromeos::MountEventType evt,
                    const std::string& path);

  void ScanForDevices(chromeos::MountLibrary* obj);

 private:
  friend struct DefaultSingletonTraits<USBMountObserver>;
  typedef std::vector<BrowserWithPath>::iterator BrowserIterator;
  BrowserIterator FindBrowserForPath(const std::string& path);

  USBMountObserver() {}
  ~USBMountObserver() {}

  void RemoveBrowserFromVector(const std::string& path);

  // Used to create a window of a standard size, and add it to a list
  // of tracked browser windows in case that device goes away.
  void OpenFileBrowse(const std::string& url,
                      const std::string& device_path,
                      bool small);

  std::vector<BrowserWithPath> browsers_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(USBMountObserver);
};

} // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
