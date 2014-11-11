// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/usb_device_permission.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/usb_ids.h"
#include "extensions/common/permissions/permissions_info.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

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

  if (data_set_.size() == 1) {
    const UsbDevicePermissionData& data = *data_set_.begin();

    const char* vendor = device::UsbIds::GetVendorName(data.vendor_id());
    if (vendor) {
      const char* product =
          device::UsbIds::GetProductName(data.vendor_id(), data.product_id());
      if (product) {
        result.push_back(PermissionMessage(
            PermissionMessage::kUsbDevice,
            l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE,
                                       base::UTF8ToUTF16(product),
                                       base::UTF8ToUTF16(vendor))));
      } else {
        result.push_back(PermissionMessage(
            PermissionMessage::kUsbDevice,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_PRODUCT,
                base::UTF8ToUTF16(vendor))));
      }
    } else {
      result.push_back(PermissionMessage(
          PermissionMessage::kUsbDevice,
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_VENDOR)));
    }
  } else if (data_set_.size() > 1) {
    std::vector<base::string16> details;
    std::set<uint16> unknown_product_vendors;
    bool found_unknown_vendor = false;

    for (const UsbDevicePermissionData& data : data_set_) {
      const char* vendor = device::UsbIds::GetVendorName(data.vendor_id());
      if (vendor) {
        const char* product =
            device::UsbIds::GetProductName(data.vendor_id(), data.product_id());
        if (product) {
          details.push_back(l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM,
              base::UTF8ToUTF16(product), base::UTF8ToUTF16(vendor)));
        } else {
          unknown_product_vendors.insert(data.vendor_id());
        }
      } else {
        found_unknown_vendor = true;
      }
    }

    // List generic "devices from this vendor" entries after specific devices.
    for (const uint16& vendor_id : unknown_product_vendors) {
      const char* vendor = device::UsbIds::GetVendorName(vendor_id);
      DCHECK(vendor);
      details.push_back(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM_UNKNOWN_PRODUCT,
          base::UTF8ToUTF16(vendor)));
    }

    // Display the catch all "device from an unknown vendor" last.
    if (found_unknown_vendor) {
      details.push_back(l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM_UNKNOWN_VENDOR));
    }

    result.push_back(PermissionMessage(
        PermissionMessage::kUsbDevice,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST),
        JoinString(details, base::char16('\n'))));
  }

  return result;
}

}  // namespace extensions
