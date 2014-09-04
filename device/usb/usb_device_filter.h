// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_FILTER_H_
#define DEVICE_USB_USB_DEVICE_FILTER_H_

#include <vector>

#include "base/memory/ref_counted.h"

namespace base {
class Value;
}

namespace device {

class UsbDevice;

class UsbDeviceFilter {
 public:
  UsbDeviceFilter();
  ~UsbDeviceFilter();

  void SetVendorId(uint16 vendor_id);
  void SetProductId(uint16 product_id);
  void SetInterfaceClass(uint8 interface_class);
  void SetInterfaceSubclass(uint8 interface_subclass);
  void SetInterfaceProtocol(uint8 interface_protocol);

  bool Matches(scoped_refptr<UsbDevice> device) const;
  base::Value* ToValue() const;

  static bool MatchesAny(scoped_refptr<UsbDevice> device,
                         const std::vector<UsbDeviceFilter>& filters);

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

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_FILTER_H_
