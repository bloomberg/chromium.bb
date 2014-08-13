// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_DEVICE_FILTER_H_
#define COMPONENTS_USB_SERVICE_USB_DEVICE_FILTER_H_

#include "base/memory/ref_counted.h"
#include "components/usb_service/usb_service_export.h"

namespace base {
class Value;
}

namespace usb_service {

class UsbDevice;

class USB_SERVICE_EXPORT UsbDeviceFilter {
 public:
  UsbDeviceFilter();
  ~UsbDeviceFilter();

  void SetVendorId(uint16 vendor_id);
  void SetProductId(uint16 product_id);
  void SetInterfaceClass(uint8 interface_class);
  void SetInterfaceSubclass(uint8 interface_subclass);
  void SetInterfaceProtocol(uint8 interface_protocol);

  bool Matches(scoped_refptr<UsbDevice> device);
  base::Value* ToValue() const;

 private:
  uint16 vendor_id_;
  uint16 product_id_;
  uint8 interface_class_;
  uint8 interface_subclass_;
  uint8 interface_protocol_;
  bool vendor_id_set_ : 1;
  bool product_id_set_ : 1;
  bool interface_class_set_ : 1;
  bool interface_subclass_set_ : 1;
  bool interface_protocol_set_ : 1;
};

}  // namespace usb_service

#endif  // COMPONENTS_USB_SERVICE_USB_DEVICE_FILTER_H_
