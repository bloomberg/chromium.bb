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
#include "device/usb/public/interfaces/device_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace device {

class UsbDevice;
class UsbDeviceFilter;
class UsbDeviceHandle;

namespace usb {

class DeviceManagerDelegate;

// Implementation of the public DeviceManager interface. Clients in the browser
// can connect to this service via DeviceClient::ConnectToUSBDeviceManager.
class DeviceManagerImpl : public DeviceManager {
 public:
  DeviceManagerImpl(mojo::InterfaceRequest<DeviceManager> request,
                    scoped_ptr<DeviceManagerDelegate> delegate);
  ~DeviceManagerImpl() override;

 private:
  // DeviceManager implementation:
  void GetDevices(EnumerationOptionsPtr options,
                  const GetDevicesCallback& callback) override;
  void OpenDevice(const mojo::String& guid,
                  mojo::InterfaceRequest<Device> device_request,
                  const OpenDeviceCallback& callback) override;

  // Callback to handle the async response from the underlying UsbService.
  void OnGetDevices(const GetDevicesCallback& callback,
                    const std::vector<UsbDeviceFilter>& filters,
                    const std::vector<scoped_refptr<UsbDevice>>& devices);

  // Callback to handle the async Open response from a UsbDevice.
  void OnOpenDevice(const OpenDeviceCallback& callback,
                    mojo::InterfaceRequest<Device> device_request,
                    scoped_refptr<UsbDeviceHandle> device_handle);

  mojo::StrongBinding<DeviceManager> binding_;

  scoped_ptr<DeviceManagerDelegate> delegate_;

  base::WeakPtrFactory<DeviceManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerImpl);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_DEVICE_MANAGER_IMPL_H_
