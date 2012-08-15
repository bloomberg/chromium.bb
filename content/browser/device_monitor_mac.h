// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_
#define CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_

#include <IOKit/IOKitLib.h>

#include <vector>

#include "base/basictypes.h"
#include "base/system_monitor/system_monitor.h"

namespace content {

class DeviceMonitorMac {
 public:
  DeviceMonitorMac();
  ~DeviceMonitorMac();

 private:
  void RegisterAudioServices();
  void RegisterVideoServices();

  static void AudioDeviceCallback(void *context, io_iterator_t iterator);
  static void VideoDeviceCallback(void *context, io_iterator_t iterator);

  // Forward the notifications to system monitor.
  void NotifyDeviceChanged(base::SystemMonitor::DeviceType type);

  // Helper.
  void RegisterServices(CFMutableDictionaryRef dictionary,
                        IOServiceMatchingCallback callback);

  IONotificationPortRef notification_port_;
  std::vector<io_iterator_t*> notification_iterators_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMonitorMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_MONITOR_MAC_H_
