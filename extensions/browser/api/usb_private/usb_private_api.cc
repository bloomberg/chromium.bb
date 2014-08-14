// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb_private/usb_private_api.h"

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "components/usb_service/usb_device_filter.h"
#include "components/usb_service/usb_device_handle.h"
#include "components/usb_service/usb_service.h"
#include "device/usb/usb_ids.h"
#include "extensions/common/api/usb_private.h"

namespace usb_private = extensions::core_api::usb_private;
namespace GetDevices = usb_private::GetDevices;
namespace GetDeviceInfo = usb_private::GetDeviceInfo;

using usb_service::UsbDevice;
using usb_service::UsbDeviceFilter;
using usb_service::UsbDeviceHandle;
using usb_service::UsbService;

namespace {

const char kErrorInitService[] = "Failed to initialize USB service.";
const char kErrorNoDevice[] = "No such device.";
const char kErrorOpen[] = "Failed to open device.";

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
  UsbService* service = UsbService::GetInstance();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return;
  }

  std::vector<UsbDeviceFilter> filters;
  filters.resize(parameters_->filters.size());
  for (size_t i = 0; i < parameters_->filters.size(); ++i) {
    UsbDeviceFilter& filter = filters[i];
    const usb_private::DeviceFilter* filter_param =
        parameters_->filters[i].get();

    if (filter_param->vendor_id) {
      filter.SetVendorId(*filter_param->vendor_id);
    }
    if (filter_param->product_id) {
      filter.SetProductId(*filter_param->product_id);
    }
    if (filter_param->interface_class) {
      filter.SetInterfaceClass(*filter_param->interface_class);
    }
    if (filter_param->interface_subclass) {
      filter.SetInterfaceSubclass(*filter_param->interface_subclass);
    }
    if (filter_param->interface_protocol) {
      filter.SetInterfaceProtocol(*filter_param->interface_protocol);
    }
  }

  std::vector<scoped_refptr<UsbDevice> > devices;
  service->GetDevices(&devices);

  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (size_t i = 0; i < devices.size(); ++i) {
    scoped_refptr<UsbDevice> device = devices[i];
    bool matched = false;

    if (filters.empty()) {
      matched = true;
    } else {
      for (size_t j = 0; !matched && j < filters.size(); ++j) {
        if (filters[j].Matches(device)) {
          matched = true;
        }
      }
    }

    if (matched) {
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
  UsbService* service = UsbService::GetInstance();
  if (!service) {
    CompleteWithError(kErrorInitService);
    return;
  }

  scoped_refptr<UsbDevice> device =
      service->GetDeviceById(parameters_->device_id);
  if (!device) {
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

  scoped_refptr<UsbDeviceHandle> device_handle = device->Open();
  if (!device_handle) {
    CompleteWithError(kErrorOpen);
    return;
  }

  base::string16 utf16;
  if (device_handle->GetManufacturer(&utf16)) {
    device_info.manufacturer_string.reset(
        new std::string(base::UTF16ToUTF8(utf16)));
  }

  if (device_handle->GetProduct(&utf16)) {
    device_info.product_string.reset(new std::string(base::UTF16ToUTF8(utf16)));
  }

  if (device_handle->GetSerial(&utf16)) {
    device_info.serial_string.reset(new std::string(base::UTF16ToUTF8(utf16)));
  }

  SetResult(device_info.ToValue().release());
  AsyncWorkCompleted();
}

}  // namespace extensions
