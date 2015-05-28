// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_PUBLIC_CPP_DEVICE_MANAGER_FACTORY_H_
#define DEVICE_USB_PUBLIC_CPP_DEVICE_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "device/usb/public/interfaces/device_manager.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace device {
namespace usb {

class DeviceManagerDelegate;

// Public interface to construct an implementation of the DeviceManager service.
class DeviceManagerFactory {
 public:
  // Builds a new DeviceManager instance to fulfill a request. The service is
  // bound to the lifetime of |request|'s MessagePipe, and it takes ownership of
  // |delegate|.
  static void Build(mojo::InterfaceRequest<DeviceManager> request,
                    scoped_ptr<DeviceManagerDelegate> delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceManagerFactory);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_PUBLIC_CPP_DEVICE_MANAGER_FACTORY_H_
