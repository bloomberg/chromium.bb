// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_device_manager.h"

#include <utility>

#include "base/bind.h"
#include "device/base/device_client.h"
#include "device/usb/mojo/device_manager_impl.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/usb_device.h"

// static
void WebUsbDeviceManager::Create(
    base::WeakPtr<device::usb::PermissionProvider> permission_provider,
    device::mojom::UsbDeviceManagerRequest request) {
  // Bind the Blink request with WebUsbDeviceManager.
  auto* web_usb_device_manager =
      new WebUsbDeviceManager(std::move(permission_provider));
  web_usb_device_manager->binding_ = mojo::MakeStrongBinding(
      base::WrapUnique(web_usb_device_manager), std::move(request));
}

WebUsbDeviceManager::WebUsbDeviceManager(
    base::WeakPtr<device::usb::PermissionProvider> permission_provider)
    : permission_provider_(std::move(permission_provider)),
      observer_(this),
      weak_factory_(this) {
  // Bind |device_manager_| to UsbDeviceManager and set error handler.
  // TODO(donna.wu@intel.com): Request UsbDeviceManagerPtr from the Device
  // Service after moving //device/usb to //services/device.
  device::usb::DeviceManagerImpl::Create(permission_provider_,
                                         mojo::MakeRequest(&device_manager_));
  device_manager_.set_connection_error_handler(base::BindOnce(
      &WebUsbDeviceManager::OnConnectionError, base::Unretained(this)));
  // Listen for add/remove device events from UsbService.
  // TODO(donna.wu@intel.com): Listen to |device_manager_| in the future.
  // We can't set WebUsbDeviceManager as a UsbDeviceManagerClient because
  // the OnDeviceRemoved event will be delivered here after it is delivered
  // to UsbChooserContext, meaning that all ephemeral permission checks in
  // OnDeviceRemoved() will fail.
  auto* usb_service = device::DeviceClient::Get()->GetUsbService();
  if (usb_service)
    observer_.Add(usb_service);
}

WebUsbDeviceManager::~WebUsbDeviceManager() = default;

void WebUsbDeviceManager::GetDevices(
    device::mojom::UsbEnumerationOptionsPtr options,
    GetDevicesCallback callback) {
  device_manager_->GetDevices(std::move(options), std::move(callback));
}

void WebUsbDeviceManager::GetDevice(
    const std::string& guid,
    device::mojom::UsbDeviceRequest device_request) {
  device_manager_->GetDevice(guid, std::move(device_request));
}

void WebUsbDeviceManager::SetClient(
    device::mojom::UsbDeviceManagerClientPtr client) {
  client_ = std::move(client);
}

void WebUsbDeviceManager::OnDeviceAdded(
    scoped_refptr<device::UsbDevice> device) {
  if (client_ && permission_provider_ &&
      permission_provider_->HasDevicePermission(device)) {
    client_->OnDeviceAdded(device::mojom::UsbDeviceInfo::From(*device));
  }
}

void WebUsbDeviceManager::OnDeviceRemoved(
    scoped_refptr<device::UsbDevice> device) {
  if (client_ && permission_provider_ &&
      permission_provider_->HasDevicePermission(device)) {
    client_->OnDeviceRemoved(device::mojom::UsbDeviceInfo::From(*device));
  }
}

void WebUsbDeviceManager::OnConnectionError() {
  device_manager_.reset();

  // Close the connection with blink.
  client_.reset();
  binding_->Close();
}

void WebUsbDeviceManager::WillDestroyUsbService() {
  OnConnectionError();
}
