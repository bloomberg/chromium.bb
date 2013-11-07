// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_
#define CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_

#include "base/basictypes.h"
#include "base/system_monitor/system_monitor.h"

namespace {
class DeviceMonitorMacImpl;
}

namespace content {

// Class to track audio/video devices removal or addition via callback to
// base::SystemMonitor ProcessDevicesChanged(). A single object of this class
// is created from the browser main process and lives as long as this one.
class DeviceMonitorMac {
 public:
  DeviceMonitorMac();
  ~DeviceMonitorMac();

  // Method called by the internal DeviceMonitorMacImpl object
  // |device_monitor_impl_| when a device of type |type| has been added to or
  // removed from the system. This code executes in the notification thread
  // (QTKit or AVFoundation).
  void NotifyDeviceChanged(base::SystemMonitor::DeviceType type);

 private:
  scoped_ptr<DeviceMonitorMacImpl> device_monitor_impl_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMonitorMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_
