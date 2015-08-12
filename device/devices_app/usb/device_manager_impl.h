// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_DEVICE_MANAGER_IMPL_H_
#define DEVICE_USB_DEVICE_MANAGER_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/devices_app/usb/public/interfaces/device_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class UsbDevice;
class UsbDeviceFilter;
class UsbDeviceHandle;

namespace usb {

class DeviceManagerDelegate;

// Implementation of the public DeviceManager interface. This interface can be
// requested from the devices app located at "mojo:devices", if available.
class DeviceManagerImpl : public DeviceManager {
 public:
  DeviceManagerImpl(
      mojo::InterfaceRequest<DeviceManager> request,
      scoped_ptr<DeviceManagerDelegate> delegate,
      scoped_refptr<base::SequencedTaskRunner> service_task_runner);
  ~DeviceManagerImpl() override;

  void set_connection_error_handler(const mojo::Closure& error_handler);

 private:
  // DeviceManager implementation:
  void GetDevices(EnumerationOptionsPtr options,
                  const GetDevicesCallback& callback) override;
  void OpenDevice(const mojo::String& guid,
                  mojo::InterfaceRequest<Device> device_request,
                  const OpenDeviceCallback& callback) override;

  // Callback to handle the async response from the underlying UsbService.
  void OnGetDevices(const GetDevicesCallback& callback,
                    mojo::Array<DeviceInfoPtr> devices);

  mojo::StrongBinding<DeviceManager> binding_;

  scoped_ptr<DeviceManagerDelegate> delegate_;
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;

  base::WeakPtrFactory<DeviceManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerImpl);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_DEVICE_MANAGER_IMPL_H_
