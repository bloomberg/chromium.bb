// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_LINUX_H_
#define DEVICE_HID_HID_SERVICE_LINUX_H_

#include <libudev.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_libevent.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

namespace device {

class HidConnection;

template<typename T, void func(T*)>
void DiscardReturnType(T* arg) { func(arg); }
template<typename T, T* func(T*)>
void DiscardReturnType(T* arg) { func(arg); }

template<typename T, void func(T*)>
struct Deleter {
  void operator()(T* enumerate) const {
    if (enumerate != NULL)
      func(enumerate);
  }
};

typedef Deleter<
    udev_enumerate,
    DiscardReturnType<udev_enumerate, udev_enumerate_unref> >
    UdevEnumerateDeleter;
typedef Deleter<
    udev_device,
    DiscardReturnType<udev_device, udev_device_unref> > UdevDeviceDeleter;
typedef Deleter<udev, DiscardReturnType<udev, udev_unref> > UdevDeleter;
typedef Deleter<
    udev_monitor,
    DiscardReturnType<udev_monitor, udev_monitor_unref> > UdevMonitorDeleter;

typedef scoped_ptr<udev_device, UdevDeviceDeleter> ScopedUdevDevicePtr;
typedef scoped_ptr<udev_enumerate, UdevEnumerateDeleter> ScopedUdevEnumeratePtr;

class HidServiceLinux : public HidService,
                        public base::MessagePumpLibevent::Watcher {
 public:
  HidServiceLinux();

  virtual scoped_refptr<HidConnection> Connect(std::string device_id) OVERRIDE;

  // Implements base::MessagePumpLibevent::Watcher
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  virtual ~HidServiceLinux();

  void Enumerate();
  void PlatformDeviceAdd(udev_device* device);
  void PlatformDeviceRemove(udev_device* raw_dev);

  scoped_ptr<udev, UdevDeleter> udev_;
  scoped_ptr<udev_monitor, UdevMonitorDeleter> monitor_;
  int monitor_fd_;
  base::MessagePumpLibevent::FileDescriptorWatcher monitor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceLinux);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_LINUX_H_
