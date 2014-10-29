// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_LINUX_H_
#define DEVICE_HID_HID_SERVICE_LINUX_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/hid/device_monitor_linux.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

struct udev_device;

namespace device {

class HidConnection;

class HidServiceLinux : public HidService,
                        public DeviceMonitorLinux::Observer {
 public:
  HidServiceLinux(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  void Connect(const HidDeviceId& device_id,
               const ConnectCallback& callback) override;

  // Implements DeviceMonitorLinux::Observer:
  void OnDeviceAdded(udev_device* device) override;
  void OnDeviceRemoved(udev_device* device) override;

 private:
  ~HidServiceLinux() override;

  void FinishConnect(const HidDeviceId& device_id,
                     const std::string device_node,
                     const ConnectCallback& callback,
                     bool success);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<HidServiceLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_LINUX_H_
