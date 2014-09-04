// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_SERVICE_H_
#define DEVICE_USB_USB_SERVICE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class UsbDevice;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsible for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService : public base::NonThreadSafe {
 public:
  // Must be called on a thread with a MessageLoopForIO (for example
  // BrowserThread::FILE). The UI task runner reference is used to talk to the
  // PermissionBrokerClient on ChromeOS (UI thread). Returns NULL when
  // initialization fails.
  static UsbService* GetInstance(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  static void SetInstanceForTest(UsbService* instance);

  virtual scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) = 0;

  // Get all of the devices attached to the system, inserting them into
  // |devices|. Clears |devices| before use. The result will be sorted by id
  // in increasing order. Must be called on FILE thread.
  virtual void GetDevices(std::vector<scoped_refptr<UsbDevice> >* devices) = 0;

 protected:
  friend struct base::DefaultDeleter<UsbService>;
  UsbService() {}
  virtual ~UsbService() {}
  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

}  // namespace device

#endif  // DEVICE_USB_USB_SERVICE_H_
