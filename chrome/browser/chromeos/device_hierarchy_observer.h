// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_HIERARCHY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_HIERARCHY_OBSERVER_H_

namespace chromeos {

// Observers receive notifications when a device has been added/removed.
class DeviceHierarchyObserver {
 public:
  virtual void DeviceHierarchyChanged() = 0;

  // Called when a new device (e.g. an external USB keyboard) is attached or
  // detached.
  virtual void DeviceAdded(int device_id) = 0;
  virtual void DeviceRemoved(int device_id) = 0;

 protected:
  virtual ~DeviceHierarchyObserver() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_HIERARCHY_OBSERVER_H_
