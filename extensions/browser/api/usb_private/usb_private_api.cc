// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb_private/usb_private_api.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_ids.h"
#include "device/usb/usb_service.h"
#include "extensions/common/api/usb_private.h"

namespace usb = extensions::core_api::usb;
namespace usb_private = extensions::core_api::usb_private;
namespace GetDevices = usb_private::GetDevices;
namespace GetDeviceInfo = usb_private::GetDeviceInfo;

using content::BrowserThread;
using device::UsbDevice;
using device::UsbDeviceFilter;
using device::UsbDeviceHandle;
using device::UsbService;

typedef std::vector<scoped_refptr<UsbDevice> > DeviceVector;

namespace {

const char kErrorInitService[] = "Failed to initialize USB service.";
const char kErrorNoDevice[] = "No such device.";

}  // namespace

namespace extensions {

UsbPrivateGetDevicesFunction::UsbPrivateGetDevicesFunction() {
}

UsbPrivateGetDevicesFunction::~UsbPrivateGetDevicesFunction() {
}

bool UsbPrivateGetDevicesFunction::Prepare() {
  parameters_ = GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbPrivateGetDevicesFunction::AsyncWorkStart() {
  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return;
  }

  std::vector<UsbDeviceFilter> filters;
  filters.resize(parameters_->filters.size());
  for (size_t i = 0; i < parameters_->filters.size(); ++i) {
    CreateDeviceFilter(*parameters_->filters[i].get(), &filters[i]);
  }

  DeviceVector devices;
  service->GetDevices(&devices);

  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (DeviceVector::iterator it = devices.begin(); it != devices.end(); ++it) {
    scoped_refptr<UsbDevice> device = *it;
    if (filters.empty() || UsbDeviceFilter::MatchesAny(device, filters)) {
      result->Append(new base::FundamentalValue((int)device->unique_id()));
    }
  }

  SetResult(result.release());
  AsyncWorkCompleted();
}

UsbPrivateGetDeviceInfoFunction::UsbPrivateGetDeviceInfoFunction() {
}

UsbPrivateGetDeviceInfoFunction::~UsbPrivateGetDeviceInfoFunction() {
}

bool UsbPrivateGetDeviceInfoFunction::Prepare() {
  parameters_ = GetDeviceInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());
  return true;
}

void UsbPrivateGetDeviceInfoFunction::AsyncWorkStart() {
  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return;
  }

  scoped_refptr<UsbDevice> device =
      service->GetDeviceById(parameters_->device_id);
  if (!device.get()) {
    CompleteWithError(kErrorNoDevice);
    return;
  }

  usb_private::DeviceInfo device_info;
  device_info.vendor_id = device->vendor_id();
  device_info.product_id = device->product_id();

  const char* name = device::UsbIds::GetVendorName(device_info.vendor_id);
  if (name) {
    device_info.vendor_name.reset(new std::string(name));
  }

  name = device::UsbIds::GetProductName(device_info.vendor_id,
                                        device_info.product_id);
  if (name) {
    device_info.product_name.reset(new std::string(name));
  }

  base::string16 utf16;
  if (device->GetManufacturer(&utf16)) {
    device_info.manufacturer_string.reset(
        new std::string(base::UTF16ToUTF8(utf16)));
  }

  if (device->GetProduct(&utf16)) {
    device_info.product_string.reset(new std::string(base::UTF16ToUTF8(utf16)));
  }

  if (device->GetSerialNumber(&utf16)) {
    device_info.serial_string.reset(new std::string(base::UTF16ToUTF8(utf16)));
  }

  SetResult(device_info.ToValue().release());
  AsyncWorkCompleted();
}

}  // namespace extensions
