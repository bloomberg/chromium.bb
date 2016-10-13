// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/usb_internals/usb_internals_page_handler.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "device/base/device_client.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_service.h"
#include "device/usb/webusb_descriptors.h"
#include "url/gurl.h"

namespace {

class TestUsbDevice : public device::UsbDevice {
 public:
  TestUsbDevice(const std::string& name,
                const std::string& serial_number,
                const GURL& landing_page,
                const GURL& allowed_origin);

  // device::UsbDevice overrides:
  void Open(const OpenCallback& callback) override;

 private:
  ~TestUsbDevice() override;

  DISALLOW_COPY_AND_ASSIGN(TestUsbDevice);
};

TestUsbDevice::TestUsbDevice(const std::string& name,
                             const std::string& serial_number,
                             const GURL& landing_page,
                             const GURL& allowed_origin)
    : UsbDevice(0x0210,
                0xff,
                0xff,
                0xff,
                0x0000,
                0x000,
                0x0100,
                base::string16(),
                base::UTF8ToUTF16(name),
                base::UTF8ToUTF16(serial_number)) {
  webusb_allowed_origins_.reset(new device::WebUsbAllowedOrigins());
  webusb_allowed_origins_->origins.push_back(allowed_origin);
  webusb_landing_page_ = landing_page;
}

void TestUsbDevice::Open(const OpenCallback& callback) {
  callback.Run(nullptr);
}

TestUsbDevice::~TestUsbDevice() {}

}  // namespace

UsbInternalsPageHandler::UsbInternalsPageHandler(
    mojom::UsbInternalsPageHandlerRequest request)
    : binding_(this, std::move(request)) {}

UsbInternalsPageHandler::~UsbInternalsPageHandler() {}

void UsbInternalsPageHandler::AddDeviceForTesting(
    const std::string& name,
    const std::string& serial_number,
    const std::string& landing_page,
    const std::string& allowed_origin,
    const AddDeviceForTestingCallback& callback) {
  device::UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (service) {
    GURL landing_page_url(landing_page);
    if (!landing_page_url.is_valid()) {
      callback.Run(false, "Landing page URL is invalid.");
      return;
    }

    GURL allowed_origin_url(allowed_origin);
    if (!allowed_origin_url.is_valid()) {
      callback.Run(false, "Allowed origin is invalid.");
      return;
    }

    service->AddDeviceForTesting(new TestUsbDevice(
        name, serial_number, landing_page_url, allowed_origin_url));
    callback.Run(true, "Added.");
  } else {
    callback.Run(false, "USB service unavailable.");
  }
}

void UsbInternalsPageHandler::RemoveDeviceForTesting(
    const std::string& guid,
    const RemoveDeviceForTestingCallback& callback) {
  device::UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (service)
    service->RemoveDeviceForTesting(guid);
  callback.Run();
}

void UsbInternalsPageHandler::GetTestDevices(
    const GetTestDevicesCallback& callback) {
  std::vector<scoped_refptr<device::UsbDevice>> devices;
  device::UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (service)
    service->GetTestDevices(&devices);
  std::vector<mojom::TestDeviceInfoPtr> result;
  result.reserve(devices.size());
  for (const auto& device : devices) {
    auto device_info = mojom::TestDeviceInfo::New();
    device_info->guid = device->guid();
    device_info->name = base::UTF16ToUTF8(device->product_string());
    device_info->serial_number = base::UTF16ToUTF8(device->serial_number());
    device_info->landing_page = device->webusb_landing_page();
    if (device->webusb_allowed_origins() &&
        !device->webusb_allowed_origins()->origins.empty()) {
      device_info->allowed_origin =
          url::Origin(device->webusb_allowed_origins()->origins.front());
    }
    result.push_back(std::move(device_info));
  }
  callback.Run(std::move(result));
}
