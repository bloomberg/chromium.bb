// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_WEB_USB_PERMISSION_PROVIDER_H_
#define CHROME_BROWSER_USB_WEB_USB_PERMISSION_PROVIDER_H_

#include <stdint.h>

#include "device/usb/public/interfaces/permission_provider.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"

namespace content {
class RenderFrameHost;
}

// This implementation of the permission provider interface enforces the rules
// of the WebUSB permission model. Devices are checked for WebUSB descriptors
// granting access to the render frame's current origin as well as permission
// granted by the user through a device chooser UI.
class WebUSBPermissionProvider : public device::usb::PermissionProvider {
 public:
  explicit WebUSBPermissionProvider(
      content::RenderFrameHost* render_frame_host);

  ~WebUSBPermissionProvider() override;

  // device::usb::PermissionProvider implementation.
  void HasDevicePermission(
      mojo::Array<device::usb::DeviceInfoPtr> requested_devices,
      const HasDevicePermissionCallback& callback) override;
  void HasConfigurationPermission(
      uint8_t requested_configuration,
      device::usb::DeviceInfoPtr device,
      const HasInterfacePermissionCallback& callback) override;
  void HasInterfacePermission(
      uint8_t requested_interface,
      uint8_t configuration_value,
      device::usb::DeviceInfoPtr device,
      const HasInterfacePermissionCallback& callback) override;
  void Bind(
      mojo::InterfaceRequest<device::usb::PermissionProvider> request) override;

 private:
  mojo::WeakBindingSet<PermissionProvider> bindings_;
  content::RenderFrameHost* const render_frame_host_;
};

#endif  // CHROME_BROWSER_USB_WEB_USB_PERMISSION_PROVIDER_H_
