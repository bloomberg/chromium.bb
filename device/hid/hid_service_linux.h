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

#if defined(OS_CHROMEOS)
  virtual void RequestAccess(
      const HidDeviceId& device_id,
      const base::Callback<void(bool success)>& callback) override;
#endif

  virtual scoped_refptr<HidConnection> Connect(const HidDeviceId& device_id)
      override;

  // Implements DeviceMonitorLinux::Observer:
  virtual void OnDeviceAdded(udev_device* device) override;
  virtual void OnDeviceRemoved(udev_device* device) override;

 private:
  virtual ~HidServiceLinux();

  void OnRequestAccessComplete(
      const base::Callback<void(bool success)>& callback,
      bool success);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<HidServiceLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_LINUX_H_
