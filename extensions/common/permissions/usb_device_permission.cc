// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/usb_device_permission.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/permissions_info.h"
#include "grit/extensions_strings.h"
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
  // //device/usb/usb.gyp:device_usb is not available when extensions are
  // disabled.
  for (std::set<UsbDevicePermissionData>::const_iterator i =
      data_set_.begin(); i != data_set_.end(); ++i) {
    const char* vendor = device::UsbIds::GetVendorName(i->vendor_id());

    if (vendor) {
      const char* product =
          device::UsbIds::GetProductName(i->vendor_id(), i->product_id());
      if (product) {
        result.push_back(PermissionMessage(
            PermissionMessage::kUsbDevice,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE,
                base::ASCIIToUTF16(product),
                base::ASCIIToUTF16(vendor))));
      } else {
        result.push_back(PermissionMessage(
            PermissionMessage::kUsbDevice,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_MISSING_PRODUCT,
                base::ASCIIToUTF16(vendor))));
      }
    } else {
      result.push_back(PermissionMessage(
          PermissionMessage::kUsbDevice,
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_MISSING_VENDOR)));
    }
  }
#else
  NOTREACHED();
#endif  // defined(ENABLE_EXTENSIONS)

  return result;
}

}  // namespace extensions
