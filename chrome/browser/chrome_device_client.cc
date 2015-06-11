// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_device_client.h"

#include "base/logging.h"
#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_service.h"
#include "device/usb/public/cpp/device_manager_delegate.h"
#include "device/usb/public/cpp/device_manager_factory.h"
#include "device/usb/public/interfaces/device.mojom.h"
#include "device/usb/usb_service.h"

using content::BrowserThread;

namespace {

// DeviceManagerDelegate implementation which allows access to all devices.
class BrowserDeviceManagerDelegate : public device::usb::DeviceManagerDelegate {
 public:
  BrowserDeviceManagerDelegate() {}
  ~BrowserDeviceManagerDelegate() override {}

 private:
  bool IsDeviceAllowed(const device::usb::DeviceInfo& device) override {
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserDeviceManagerDelegate);
};

}  // namespace

ChromeDeviceClient::ChromeDeviceClient() {}

ChromeDeviceClient::~ChromeDeviceClient() {}

device::UsbService* ChromeDeviceClient::GetUsbService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return device::UsbService::GetInstance(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

void ChromeDeviceClient::ConnectToUSBDeviceManager(
    mojo::InterfaceRequest<device::usb::DeviceManager> request) {
  device::usb::DeviceManagerFactory::Build(
      request.Pass(),
      scoped_ptr<device::usb::DeviceManagerDelegate>(
          new BrowserDeviceManagerDelegate));
}

device::HidService* ChromeDeviceClient::GetHidService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return device::HidService::GetInstance(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}
