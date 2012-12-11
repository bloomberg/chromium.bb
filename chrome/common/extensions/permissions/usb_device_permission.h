// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_USB_DEVICE_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_USB_DEVICE_PERMISSION_H_

#include "base/basictypes.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/set_disjunction_permission.h"
#include "chrome/common/extensions/permissions/usb_device_permission_data.h"

namespace extensions {

class UsbDevicePermission
  : public SetDisjunctionPermission<UsbDevicePermissionData,
                                    UsbDevicePermission> {
 public:
  struct CheckParam : public APIPermission::CheckParam {
    CheckParam(uint16 vendor_id, uint16 product_id)
      : vendor_id(vendor_id), product_id(product_id) {}
    const uint16 vendor_id;
    const uint16 product_id;
  };

  explicit UsbDevicePermission(const APIPermissionInfo* info);
  virtual ~UsbDevicePermission();

  // APIPermission overrides
  virtual PermissionMessages GetMessages() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_USB_DEVICE_PERMISSION_H_
