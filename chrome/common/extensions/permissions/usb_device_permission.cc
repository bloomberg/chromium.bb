// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/usb_device_permission.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_EXTENSIONS)
#include "device/usb/usb_ids.h"
#endif

namespace extensions {

UsbDevicePermission::UsbDevicePermission(
    const APIPermissionInfo* info)
  : SetDisjunctionPermission<UsbDevicePermissionData,
                             UsbDevicePermission>(info) {
}

UsbDevicePermission::~UsbDevicePermission() {
}

PermissionMessages UsbDevicePermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;

#if defined(ENABLE_EXTENSIONS)
  // device.gyp:device_usb is not available when extensions are disabled.
  for (std::set<UsbDevicePermissionData>::const_iterator i =
      data_set_.begin(); i != data_set_.end(); ++i) {

    const char* vendor = device::UsbIds::GetVendorName(i->vendor_id());
    string16 vendor_name;
    if (vendor) {
      vendor_name = ASCIIToUTF16(vendor);
    } else {
      vendor_name = l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_WARNING_UNKNOWN_USB_VENDOR);
    }

    const char* product =
        device::UsbIds::GetProductName(i->vendor_id(), i->product_id());
    string16 product_name;
    if (product) {
      product_name = ASCIIToUTF16(product);
    } else {
      product_name = l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_WARNING_UNKNOWN_USB_PRODUCT);
    }

    result.push_back(PermissionMessage(
          PermissionMessage::kUsbDevice,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE,
              product_name,
              vendor_name)));
  }
#else
  NOTREACHED();
#endif  // defined(ENABLE_EXTENSIONS)

  return result;
}

}  // namespace extensions
