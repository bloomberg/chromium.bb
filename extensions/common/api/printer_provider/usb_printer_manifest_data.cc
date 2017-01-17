// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/printer_provider/usb_printer_manifest_data.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/manifest_constants.h"

using device::UsbDeviceFilter;

namespace extensions {

UsbPrinterManifestData::UsbPrinterManifestData() {
}

UsbPrinterManifestData::~UsbPrinterManifestData() {
}

// static
const UsbPrinterManifestData* UsbPrinterManifestData::Get(
    const Extension* extension) {
  return static_cast<UsbPrinterManifestData*>(
      extension->GetManifestData(manifest_keys::kUsbPrinters));
}

// static
std::unique_ptr<UsbPrinterManifestData> UsbPrinterManifestData::FromValue(
    const base::Value& value,
    base::string16* error) {
  std::unique_ptr<api::extensions_manifest_types::UsbPrinters> usb_printers =
      api::extensions_manifest_types::UsbPrinters::FromValue(value, error);
  if (!usb_printers) {
    return nullptr;
  }

  auto result = base::MakeUnique<UsbPrinterManifestData>();
  for (const auto& input : usb_printers->filters) {
    if (input.product_id && input.interface_class) {
      *error = base::ASCIIToUTF16(
          "Only one of productId or interfaceClass may be specified.");
      return nullptr;
    }

    UsbDeviceFilter output;
    output.vendor_id = input.vendor_id;

    if (input.product_id)
      output.product_id = *input.product_id;

    if (input.interface_class) {
      output.interface_class = *input.interface_class;
      if (input.interface_subclass) {
        output.interface_subclass = *input.interface_subclass;
        if (input.interface_protocol)
          output.interface_protocol = *input.interface_protocol;
      }
    }

    result->filters_.push_back(output);
  }
  return result;
}

bool UsbPrinterManifestData::SupportsDevice(
    const scoped_refptr<device::UsbDevice>& device) const {
  for (const auto& filter : filters_) {
    if (filter.Matches(device))
      return true;
  }

  return false;
}

}  // namespace extensions
