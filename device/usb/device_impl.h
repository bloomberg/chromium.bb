// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_DEVICE_IMPL_H_
#define DEVICE_USB_DEVICE_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace device {

class UsbDevice;

namespace usb {

// Implementation of the public Device interface. Instances of this class are
// constructed by DeviceManagerImpl and are strongly bound to their MessagePipe
// lifetime.
class DeviceImpl : public Device {
 public:
  DeviceImpl(scoped_refptr<UsbDevice> device,
             mojo::InterfaceRequest<Device> request);
  ~DeviceImpl() override;

 private:
  // Device implementation:
  void GetDeviceInfo(const GetDeviceInfoCallback& callback) override;

  mojo::StrongBinding<Device> binding_;

  scoped_refptr<UsbDevice> device_;
  DeviceInfoPtr info_;

  DISALLOW_COPY_AND_ASSIGN(DeviceImpl);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_DEVICE_IMPL_H_
