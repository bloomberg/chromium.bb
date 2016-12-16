// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_blocklist.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/variations/variations_associated_data.h"
#include "device/usb/usb_device.h"

namespace {

static base::LazyInstance<UsbBlocklist>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// The == and < operators are necessary to use Entries in std::set.
bool operator==(const UsbBlocklist::Entry& a, const UsbBlocklist::Entry& b) {
  return a.vendor_id == b.vendor_id && a.product_id == b.product_id &&
         a.version == b.version;
}

bool operator<(const UsbBlocklist::Entry& a, const UsbBlocklist::Entry& b) {
  if (a.vendor_id == b.vendor_id) {
    if (a.product_id == b.product_id)
      return a.version < b.version;
    return a.product_id < b.product_id;
  }
  return a.vendor_id < b.vendor_id;
}

UsbBlocklist::Entry::Entry(uint16_t vendor_id,
                           uint16_t product_id,
                           uint16_t version)
    : vendor_id(vendor_id), product_id(product_id), version(version) {}

UsbBlocklist::~UsbBlocklist() {}

// static
UsbBlocklist& UsbBlocklist::Get() {
  return g_singleton.Get();
}

void UsbBlocklist::Exclude(const Entry& entry) {
  blocklisted_devices_.insert(entry);
}

void UsbBlocklist::Exclude(base::StringPiece blocklist_string) {
  for (const auto& entry :
       base::SplitStringPiece(blocklist_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<base::StringPiece> components = base::SplitStringPiece(
        entry, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (components.size() != 3 || components[0].size() != 4 ||
        components[1].size() != 4 || components[2].size() != 4) {
      continue;
    }

    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t version;
    if (!base::HexStringToUInt(components[0], &vendor_id) ||
        !base::HexStringToUInt(components[1], &product_id) ||
        !base::HexStringToUInt(components[2], &version)) {
      continue;
    }

    Exclude(Entry(vendor_id, product_id, version));
  }
}

bool UsbBlocklist::IsExcluded(const Entry& entry) {
  return base::ContainsValue(blocklisted_devices_, entry);
}

bool UsbBlocklist::IsExcluded(scoped_refptr<const device::UsbDevice> device) {
  return IsExcluded(Entry(device->vendor_id(), device->product_id(),
                          device->device_version()));
}

void UsbBlocklist::ResetToDefaultValuesForTest() {
  blocklisted_devices_.clear();
  PopulateWithDefaultValues();
  PopulateWithServerProvidedValues();
}

UsbBlocklist::UsbBlocklist() {
  PopulateWithDefaultValues();
  PopulateWithServerProvidedValues();
}

void UsbBlocklist::PopulateWithDefaultValues() {
  // To add a device to the blocklist add an entry here as well as configuring
  // a Finch trial so that the blocklist update is pushed out to existing users
  // as quickly as possible, e.g.:
  //
  // Exclude({ 0x18D0, 0x58F0, 0x1BAD });
}

void UsbBlocklist::PopulateWithServerProvidedValues() {
  std::string blocklist_string = variations::GetVariationParamValue(
      "WebUSBBlocklist", "blocklist_additions");
  Exclude(blocklist_string);
}
