// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/mount_library.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_registrar.h"

class Browser;
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
  };

  USBMountObserver() {}
  ~USBMountObserver() {}

  void set_profile(Profile* profile) { profile_ = profile; }

  static USBMountObserver* Get() {
    return Singleton<USBMountObserver>::get();
  }
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  void MountChanged(chromeos::MountLibrary* obj,
                    chromeos::MountEventType evt,
                    const std::string& path);
 private:
  typedef std::vector<BrowserWithPath>::iterator BrowserIterator;
  BrowserIterator FindBrowserForPath(const std::string& path);

  void RemoveBrowserFromVector(const std::string& path);

  // Used to create a window of a standard size, and add it to a list
  // of tracked browser windows in case that device goes away.
  void PopUpWindow(const std::string& url, const std::string& device_path);

  Profile* profile_;
  std::vector<BrowserWithPath> browsers_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(USBMountObserver);
};

} // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_USB_MOUNT_OBSERVER_H_
