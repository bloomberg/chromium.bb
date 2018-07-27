// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_DEVICE_MANAGER_H_
#define CHROME_BROWSER_USB_WEB_USB_DEVICE_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "device/usb/mojo/permission_provider.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {
class UsbDevice;
}

// Implements a restricted device::mojom::UsbDeviceManager interface by wrapping
// another UsbDeviceManager instance and checking requests with the provided
// device::usb::PermissionProvider.
class WebUsbDeviceManager : public device::mojom::UsbDeviceManager,
                            public device::UsbService::Observer {
 public:
  static void Create(
      base::WeakPtr<device::usb::PermissionProvider> permission_provider,
      device::mojom::UsbDeviceManagerRequest request);

  ~WebUsbDeviceManager() override;

 private:
  WebUsbDeviceManager(
      base::WeakPtr<device::usb::PermissionProvider> permission_provider);

  // DeviceManager implementation:
  void GetDevices(device::mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override;
  void GetDevice(const std::string& guid,
                 device::mojom::UsbDeviceRequest device_request) override;
  void SetClient(device::mojom::UsbDeviceManagerClientPtr client) override;

  // device::UsbService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;
  void WillDestroyUsbService() override;

  void OnConnectionError();

  base::WeakPtr<device::usb::PermissionProvider> permission_provider_;

  // Used to bind with Blink.
  mojo::StrongBindingPtr<device::mojom::UsbDeviceManager> binding_;
  device::mojom::UsbDeviceManagerClientPtr client_;

  device::mojom::UsbDeviceManagerPtr device_manager_;
  ScopedObserver<device::UsbService, device::UsbService::Observer> observer_;

  base::WeakPtrFactory<WebUsbDeviceManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbDeviceManager);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_DEVICE_MANAGER_H_
