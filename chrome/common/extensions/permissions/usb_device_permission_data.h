// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/common/extensions/permissions/api_permission.h"

namespace extensions {

// A pattern that can be used to match a USB device permission.
// Should be of the format: vendorId:productId, where both vendorId and
// productId are decimal strings representing uint16 values.
class UsbDevicePermissionData {
 public:
  UsbDevicePermissionData();
  UsbDevicePermissionData(uint16 vendor_id, uint16 product_id);

  bool Check(const APIPermission::CheckParam* param) const;
  bool Parse(const std::string& spec);
  const std::string& GetAsString() const;

  bool operator<(const UsbDevicePermissionData& rhs) const;
  bool operator==(const UsbDevicePermissionData& rhs) const;

  const uint16& vendor_id() const { return vendor_id_; }
  const uint16& product_id() const { return product_id_; }

  // These accessors are provided for IPC_STRUCT_TRAITS_MEMBER.  Please
  // think twice before using them for anything else.
  uint16& vendor_id() { return vendor_id_; }
  uint16& product_id() { return product_id_; }

 private:
  uint16 vendor_id_;
  uint16 product_id_;
  mutable std::string spec_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_
