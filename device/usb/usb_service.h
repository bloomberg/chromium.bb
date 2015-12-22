// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_SERVICE_H_
#define DEVICE_USB_USB_SERVICE_H_

#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class UsbDevice;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsible for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService : public base::NonThreadSafe {
 public:
  using GetDevicesCallback =
      base::Callback<void(const std::vector<scoped_refptr<UsbDevice>>&)>;

  class Observer {
   public:
    virtual ~Observer();

    // These events are delivered from the thread on which the UsbService object
    // was created.
    virtual void OnDeviceAdded(scoped_refptr<UsbDevice> device);
    virtual void OnDeviceRemoved(scoped_refptr<UsbDevice> device);
    // For observers that need to process device removal after others have run.
    // Should not depend on any other service's knowledge of connected devices.
    virtual void OnDeviceRemovedCleanup(scoped_refptr<UsbDevice> device);

    // Notifies the observer that the UsbService it depends on is shutting down.
    virtual void WillDestroyUsbService();
  };

  // The file task runner reference is used for blocking I/O operations.
  // Returns nullptr when initialization fails.
  static scoped_ptr<UsbService> Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  virtual ~UsbService();

  virtual scoped_refptr<UsbDevice> GetDevice(const std::string& guid) = 0;

  // Enumerates available devices.
  virtual void GetDevices(const GetDevicesCallback& callback) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  UsbService();

  void NotifyDeviceAdded(scoped_refptr<UsbDevice> device);
  void NotifyDeviceRemoved(scoped_refptr<UsbDevice> device);

  base::ObserverList<Observer, true> observer_list_;

 private:
  friend void base::DeletePointer<UsbService>(UsbService* service);

  DISALLOW_COPY_AND_ASSIGN(UsbService);
};

}  // namespace device

#endif  // DEVICE_USB_USB_SERVICE_H_
