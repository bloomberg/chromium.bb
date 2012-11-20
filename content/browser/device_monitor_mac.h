// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_
#define CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_

#include "base/basictypes.h"
#include "base/system_monitor/system_monitor.h"

namespace content {

class DeviceMonitorMac {
 public:
  DeviceMonitorMac();
  ~DeviceMonitorMac();

 private:
  // Forward the notifications to system monitor.
  void NotifyDeviceChanged(base::SystemMonitor::DeviceType type);

  class QTMonitorImpl;
  scoped_ptr<DeviceMonitorMac::QTMonitorImpl> qt_monitor_;
  DISALLOW_COPY_AND_ASSIGN(DeviceMonitorMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_
