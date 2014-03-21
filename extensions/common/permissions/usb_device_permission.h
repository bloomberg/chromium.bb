// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_

#include "base/basictypes.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/set_disjunction_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"

namespace extensions {

class UsbDevicePermission
  : public SetDisjunctionPermission<UsbDevicePermissionData,
                                    UsbDevicePermission> {
 public:
  struct CheckParam : public APIPermission::CheckParam {
    CheckParam(uint16 vendor_id, uint16 product_id, int interface_id)
      : vendor_id(vendor_id),
        product_id(product_id),
        interface_id(interface_id) {}
    const uint16 vendor_id;
    const uint16 product_id;
    const int interface_id;
  };

  explicit UsbDevicePermission(const APIPermissionInfo* info);
  virtual ~UsbDevicePermission();

  // APIPermission overrides
  virtual PermissionMessages GetMessages() const OVERRIDE;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_
