// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
#define CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "device/usb/mojo/permission_provider.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "device/usb/usb_service.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"

namespace device {
class UsbDevice;
}

// Implements a restricted device::mojom::UsbDeviceManager interface by wrapping
// another UsbDeviceManager instance and checking requests with the provided
// device::usb::PermissionProvider.
class WebUsbServiceImpl : public blink::mojom::WebUsbService,
                          public device::UsbService::Observer,
                          public device::mojom::UsbDeviceClient {
 public:
  static void Create(
      base::WeakPtr<device::usb::PermissionProvider> permission_provider,
      base::WeakPtr<WebUsbChooser> usb_chooser,
      blink::mojom::WebUsbServiceRequest request);

  ~WebUsbServiceImpl() override;

 private:
  WebUsbServiceImpl(
      base::WeakPtr<device::usb::PermissionProvider> permission_provider,
      base::WeakPtr<WebUsbChooser> usb_chooser);

  // blink::mojom::WebUsbService implementation:
  void GetDevices(GetDevicesCallback callback) override;
  void GetDevice(const std::string& guid,
                 device::mojom::UsbDeviceRequest device_request) override;
  void GetPermission(
      std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
      GetPermissionCallback callback) override;
  void SetClient(device::mojom::UsbDeviceManagerClientPtr client) override;

  void OnGetDevices(
      GetDevicesCallback callback,
      std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list);

  // device::UsbService::Observer implementation:
  void OnDeviceAdded(scoped_refptr<device::UsbDevice> device) override;
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

  // device::mojom::UsbDeviceClient implementation:
  void OnDeviceOpened() override;
  void OnDeviceClosed() override;

  void WillDestroyUsbService() override;

  void OnConnectionError();

  base::WeakPtr<device::usb::PermissionProvider> permission_provider_;
  base::WeakPtr<WebUsbChooser> usb_chooser_;

  // Used to bind with Blink.
  mojo::StrongBindingPtr<blink::mojom::WebUsbService> binding_;
  device::mojom::UsbDeviceManagerClientPtr client_;

  // Binding used to connect with USB devices for opened/closed events.
  mojo::BindingSet<device::mojom::UsbDeviceClient> device_client_bindings_;

  device::mojom::UsbDeviceManagerPtr device_manager_;
  ScopedObserver<device::UsbService, device::UsbService::Observer> observer_;

  base::WeakPtrFactory<WebUsbServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbServiceImpl);
};

#endif  // CHROME_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
