// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_filter.h"

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

UsbDeviceFilter::UsbDeviceFilter()
    : vendor_id_set_(false),
      product_id_set_(false),
      interface_class_set_(false),
      interface_subclass_set_(false),
      interface_protocol_set_(false) {
}

UsbDeviceFilter::~UsbDeviceFilter() {
}

void UsbDeviceFilter::SetVendorId(uint16 vendor_id) {
  vendor_id_set_ = true;
  vendor_id_ = vendor_id;
}

void UsbDeviceFilter::SetProductId(uint16 product_id) {
  product_id_set_ = true;
  product_id_ = product_id;
}

void UsbDeviceFilter::SetInterfaceClass(uint8 interface_class) {
  interface_class_set_ = true;
  interface_class_ = interface_class;
}

void UsbDeviceFilter::SetInterfaceSubclass(uint8 interface_subclass) {
  interface_subclass_set_ = true;
  interface_subclass_ = interface_subclass;
}

void UsbDeviceFilter::SetInterfaceProtocol(uint8 interface_protocol) {
  interface_protocol_set_ = true;
  interface_protocol_ = interface_protocol;
}

bool UsbDeviceFilter::Matches(scoped_refptr<UsbDevice> device) const {
  if (vendor_id_set_) {
    if (device->vendor_id() != vendor_id_) {
      return false;
    }

    if (product_id_set_ && device->product_id() != product_id_) {
      return false;
    }
  }

  if (interface_class_set_) {
    bool foundMatch = false;
    const UsbConfigDescriptor& config = device->GetConfiguration();

    // TODO(reillyg): Check device configuration if the class is not defined at
    // a per-interface level. This is not really important because most devices
    // have per-interface classes. The only counter-examples I know of are hubs.

    for (UsbInterfaceDescriptor::Iterator ifaceIt = config.interfaces.begin();
         ifaceIt != config.interfaces.end() && !foundMatch;
         ++ifaceIt) {
      if (ifaceIt->interface_class == interface_class_ &&
          (!interface_subclass_set_ ||
           (ifaceIt->interface_subclass == interface_subclass_ &&
            (!interface_protocol_set_ ||
             ifaceIt->interface_protocol == interface_protocol_)))) {
        foundMatch = true;
      }
    }

    if (!foundMatch) {
      return false;
    }
  }

  return true;
}

base::Value* UsbDeviceFilter::ToValue() const {
  scoped_ptr<base::DictionaryValue> obj(new base::DictionaryValue());

  if (vendor_id_set_) {
    obj->SetInteger(kVendorIdKey, vendor_id_);
    if (product_id_set_) {
      obj->SetInteger(kProductIdKey, product_id_);
    }
  }

  if (interface_class_set_) {
    obj->SetInteger(kInterfaceClassKey, interface_class_);
    if (interface_subclass_set_) {
      obj->SetInteger(kInterfaceSubclassKey, interface_subclass_);
      if (interface_protocol_set_) {
        obj->SetInteger(kInterfaceProtocolKey, interface_protocol_);
      }
    }
  }

  return obj.release();
}

// static
bool UsbDeviceFilter::MatchesAny(scoped_refptr<UsbDevice> device,
                                 const std::vector<UsbDeviceFilter>& filters) {
  for (std::vector<UsbDeviceFilter>::const_iterator i = filters.begin();
       i != filters.end();
       ++i) {
    if (i->Matches(device)) {
      return true;
    }
  }
  return false;
}

}  // namespace device
