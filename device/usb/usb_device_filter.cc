// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_filter.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"

namespace device {

namespace {

const char kProductIdKey[] = "productId";
const char kVendorIdKey[] = "vendorId";
const char kInterfaceClassKey[] = "interfaceClass";
const char kInterfaceSubclassKey[] = "interfaceSubclass";
const char kInterfaceProtocolKey[] = "interfaceProtocol";

}  // namespace

UsbDeviceFilter::UsbDeviceFilter() = default;

UsbDeviceFilter::UsbDeviceFilter(const UsbDeviceFilter& other) = default;

UsbDeviceFilter::~UsbDeviceFilter() = default;

bool UsbDeviceFilter::Matches(scoped_refptr<UsbDevice> device) const {
  if (vendor_id) {
    if (device->vendor_id() != *vendor_id)
      return false;

    if (product_id && device->product_id() != *product_id)
      return false;
  }

  if (serial_number &&
      device->serial_number() != base::UTF8ToUTF16(*serial_number)) {
    return false;
  }

  if (interface_class) {
    for (const UsbConfigDescriptor& config : device->configurations()) {
      for (const UsbInterfaceDescriptor& iface : config.interfaces) {
        if (iface.interface_class == *interface_class &&
            (!interface_subclass ||
             (iface.interface_subclass == *interface_subclass &&
              (!interface_protocol ||
               iface.interface_protocol == *interface_protocol)))) {
          return true;
        }
      }
    }

    return false;
  }

  return true;
}

std::unique_ptr<base::Value> UsbDeviceFilter::ToValue() const {
  auto obj = base::MakeUnique<base::DictionaryValue>();

  if (vendor_id) {
    obj->SetInteger(kVendorIdKey, *vendor_id);
    if (product_id)
      obj->SetInteger(kProductIdKey, *product_id);
  }

  if (interface_class) {
    obj->SetInteger(kInterfaceClassKey, *interface_class);
    if (interface_subclass) {
      obj->SetInteger(kInterfaceSubclassKey, *interface_subclass);
      if (interface_protocol)
        obj->SetInteger(kInterfaceProtocolKey, *interface_protocol);
    }
  }

  return std::move(obj);
}

// static
bool UsbDeviceFilter::MatchesAny(scoped_refptr<UsbDevice> device,
                                 const std::vector<UsbDeviceFilter>& filters) {
  if (filters.empty())
    return true;

  for (const auto& filter : filters) {
    if (filter.Matches(device))
      return true;
  }
  return false;
}

}  // namespace device
