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

namespace {

// Adds the permissions from the |data_set| to the permission lists that are
// not NULL. If NULL, that list is ignored.
void AddPermissionsToLists(const std::set<UsbDevicePermissionData>& data_set,
                           PermissionIDSet* ids,
                           PermissionMessages* messages) {
  // TODO(sashab): Once GetMessages() is deprecated, move this logic back into
  // GetPermissions().
  // TODO(sashab, reillyg): Once GetMessages() is deprecated, rework the
  // permission message logic for USB devices to generate more meaningful
  // messages and better fit the current rules system.
  if (data_set.size() == 1) {
    const UsbDevicePermissionData& data = *data_set.begin();

    const char* vendor = device::UsbIds::GetVendorName(data.vendor_id());
    if (vendor) {
      const char* product =
          device::UsbIds::GetProductName(data.vendor_id(), data.product_id());
      if (product) {
        base::string16 product_name_and_vendor = l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_PRODUCT_NAME_AND_VENDOR,
            base::UTF8ToUTF16(product), base::UTF8ToUTF16(vendor));

        if (messages) {
          messages->push_back(
              PermissionMessage(PermissionMessage::kUsbDevice,
                                l10n_util::GetStringFUTF16(
                                    IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE,
                                    product_name_and_vendor)));
        }
        if (ids)
          ids->insert(APIPermission::kUsbDevice, product_name_and_vendor);
      } else {
        if (messages) {
          messages->push_back(PermissionMessage(
              PermissionMessage::kUsbDevice,
              l10n_util::GetStringFUTF16(
                  IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_PRODUCT,
                  base::UTF8ToUTF16(vendor))));
        }
        if (ids) {
          ids->insert(APIPermission::kUsbDeviceUnknownProduct,
                      base::UTF8ToUTF16(vendor));
        }
      }
    } else {
      if (messages) {
        messages->push_back(PermissionMessage(
            PermissionMessage::kUsbDevice,
            l10n_util::GetStringUTF16(
                IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_VENDOR)));
      }
      if (ids) {
        ids->insert(APIPermission::kUsbDeviceUnknownVendor);
    }
    }
  } else if (data_set.size() > 1) {
    std::vector<base::string16> details;
    std::set<uint16> unknown_product_vendors;
    bool found_unknown_vendor = false;

    for (const UsbDevicePermissionData& data : data_set) {
      const char* vendor = device::UsbIds::GetVendorName(data.vendor_id());
      if (vendor) {
        const char* product =
            device::UsbIds::GetProductName(data.vendor_id(), data.product_id());
        if (product) {
          base::string16 product_name_and_vendor = l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_PRODUCT_NAME_AND_VENDOR,
              base::UTF8ToUTF16(product), base::UTF8ToUTF16(vendor));
          details.push_back(l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM,
              product_name_and_vendor));
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

    if (messages) {
      messages->push_back(
          PermissionMessage(PermissionMessage::kUsbDevice,
                            l10n_util::GetStringUTF16(
                                IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST),
                            JoinString(details, base::char16('\n'))));
    }
    if (ids) {
      for (const auto& detail : details)
        ids->insert(APIPermission::kUsbDeviceList, detail);
    }
  }
}

}  // namespace

UsbDevicePermission::UsbDevicePermission(const APIPermissionInfo* info)
    : SetDisjunctionPermission<UsbDevicePermissionData, UsbDevicePermission>(
          info) {
}

UsbDevicePermission::~UsbDevicePermission() {
}

PermissionIDSet UsbDevicePermission::GetPermissions() const {
  PermissionIDSet ids;
  AddPermissionsToLists(data_set_, &ids, NULL);
  return ids;
}

PermissionMessages UsbDevicePermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages messages;
  AddPermissionsToLists(data_set_, NULL, &messages);
  return messages;
}

}  // namespace extensions
