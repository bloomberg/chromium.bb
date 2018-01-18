// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/grit/generated_resources.h"
#include "device/usb/usb_device.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_ANDROID)
#include "device/usb/usb_ids.h"
#endif  // !defined(OS_ANDROID)

base::string16 FormatUsbDeviceName(scoped_refptr<device::UsbDevice> device) {
  base::string16 device_name = device->product_string();
  if (device_name.empty()) {
    uint16_t vendor_id = device->vendor_id();
    uint16_t product_id = device->product_id();
#if !defined(OS_ANDROID)
    if (const char* product_name =
            device::UsbIds::GetProductName(vendor_id, product_id)) {
      return base::UTF8ToUTF16(product_name);
    } else if (const char* vendor_name =
                   device::UsbIds::GetVendorName(vendor_id)) {
      return l10n_util::GetStringFUTF16(
          IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_NAME,
          base::UTF8ToUTF16(vendor_name));
    }
#endif  // !defined(OS_ANDROID)
    device_name = l10n_util::GetStringFUTF16(
        IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_ID_AND_PRODUCT_ID,
        base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id)),
        base::ASCIIToUTF16(base::StringPrintf("%04x", product_id)));
  }

  return device_name;
}

Browser* GetBrowser() {
#if defined(OS_ANDROID)
  return nullptr;
#else
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetLastUsedProfileAllowedByPolicy());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
#endif  // !defined(OS_ANDROID)
}
