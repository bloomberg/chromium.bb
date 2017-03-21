// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/usb_printer_util.h"

#include <ctype.h>
#include <stdint.h>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/printing/printer_configuration.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_filter.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace {

// Base class used for printer USB interfaces
// (https://www.usb.org/developers/defined_class).
constexpr uint8_t kPrinterInterfaceClass = 7;

// Escape URI strings the same way cups does it, so we end up with a URI cups
// recognizes.  Cups hex-encodes '%', ' ', and anything not in the standard
// ASCII range.  CUPS lets everything else through unchanged.
//
// TODO(justincarlson): Determine whether we should apply escaping at the
// outgoing edge, when we send Printer information to CUPS, instead of
// pre-escaping at the point the field is filled in.
//
// https://crbug.com/701606
std::string CupsURIEscape(const std::string& uri_in) {
  static const char kHexDigits[] = "0123456789ABCDEF";
  std::vector<char> buf;
  buf.reserve(uri_in.size());
  for (char c : uri_in) {
    if (c == ' ' || c == '%' || (c & 0x80)) {
      buf.push_back('%');
      buf.push_back(kHexDigits[(c >> 4) & 0xf]);
      buf.push_back(kHexDigits[c & 0xf]);
    } else {
      buf.push_back(c);
    }
  }
  return std::string(buf.data(), buf.size());
}

}  // namespace

bool UsbDeviceIsPrinter(scoped_refptr<device::UsbDevice> usb_device) {
  device::UsbDeviceFilter printer_filter;
  printer_filter.interface_class = kPrinterInterfaceClass;
  return printer_filter.Matches(usb_device);
}

std::string UsbPrinterDeviceDetailsAsString(const device::UsbDevice& device) {
  return base::StringPrintf(
      " guid:                %s\n"
      " usb version:         %d\n"
      " device class:        %d\n"
      " device subclass:     %d\n"
      " device protocol:     %d\n"
      " vendor id:           %04x\n"
      " product id:          %04x\n"
      " device version:      %d\n"
      " manufacturer string: %s\n"
      " product string:      %s\n"
      " serial number:       %s",
      device.guid().c_str(), device.usb_version(), device.device_class(),
      device.device_subclass(), device.device_protocol(), device.vendor_id(),
      device.product_id(), device.device_version(),
      base::UTF16ToUTF8(device.manufacturer_string()).c_str(),
      base::UTF16ToUTF8(device.product_string()).c_str(),
      base::UTF16ToUTF8(device.serial_number()).c_str());
}

// Attempt to gather all the information we need to work with this printer by
// querying the USB device.  This should only be called using devices we believe
// are printers, not arbitrary USB devices, as we may get weird partial results
// from arbitrary devices.
std::unique_ptr<Printer> UsbDeviceToPrinter(const device::UsbDevice& device) {
  // Preflight all required fields and log errors if we find something wrong.
  if (device.vendor_id() == 0 || device.product_id() == 0 ||
      device.manufacturer_string().empty() || device.product_string().empty()) {
    LOG(ERROR) << "Failed to convert USB device to printer.  Fields were:\n"
               << UsbPrinterDeviceDetailsAsString(device);
    return nullptr;
  }

  auto printer = base::MakeUnique<Printer>();
  printer->set_manufacturer(base::UTF16ToUTF8(device.manufacturer_string()));
  printer->set_model(base::UTF16ToUTF8(device.product_string()));
  printer->set_display_name(base::StringPrintf("%s %s (USB)",
                                               printer->manufacturer().c_str(),
                                               printer->model().c_str()));
  printer->set_description(printer->display_name());

  printer->set_uri(UsbPrinterUri(device));
  return printer;
}

std::string UsbPrinterUri(const device::UsbDevice& device) {
  // Note that serial may, for some devices, be empty or bogus (all zeros, non
  // unique, or otherwise not really a serial number), but having a non-unique
  // or empty serial field in the URI still lets us print, it just means we
  // don't have a way to uniquely identify a printer if there are multiple ones
  // plugged in with the same VID/PID, so we may print to the *wrong* printer.
  // There doesn't seem to be a robust solution to this problem; if printers
  // don't supply a serial number, we don't have any reliable way to do that
  // differentiation.
  std::string serial = base::UTF16ToUTF8(device.serial_number());
  return CupsURIEscape(base::StringPrintf("usb://%04x/%04x?serial=%s",
                                          device.vendor_id(),
                                          device.product_id(), serial.c_str()));
}

}  // namespace chromeos
