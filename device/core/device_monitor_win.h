// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CORE_DEVICE_MONITOR_WIN_H_
#define DEVICE_CORE_DEVICE_MONITOR_WIN_H_

#include <windows.h>

#include "base/observer_list.h"

namespace device {

// Use an instance of this class to observe devices being added and removed
// from the system, matched by device interface GUID.
class DeviceMonitorWin {
 public:
  class Observer {
   public:
    virtual void OnDeviceAdded(const std::string& device_path);
    virtual void OnDeviceRemoved(const std::string& device_path);
  };

  ~DeviceMonitorWin();

  static DeviceMonitorWin* GetForDeviceInterface(const GUID& guid);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class DeviceMonitorMessageWindow;

  DeviceMonitorWin(HDEVNOTIFY notify_handle);

  void NotifyDeviceAdded(const std::string& device_path);
  void NotifyDeviceRemoved(const std::string& device_path);

  ObserverList<Observer> observer_list_;
  HDEVNOTIFY notify_handle_;
};
}

#endif  // DEVICE_CORE_DEVICE_MONITOR_WIN_H_