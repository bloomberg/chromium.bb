// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_blocklist.h"

#include <algorithm>
#include <tuple>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/variations/variations_associated_data.h"
#include "device/usb/usb_device.h"

namespace {

static base::LazyInstance<UsbBlocklist>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

constexpr uint16_t kMaxVersion = 0xffff;

// Returns true if the passed string is exactly 4 digits long and only contains
// valid hexadecimal characters (no leading 0x).
bool IsHexComponent(base::StringPiece string) {
  if (string.length() != 4)
    return false;

  // This is necessary because base::HexStringToUInt allows whitespace and the
  // "0x" prefix in its input.
  for (char c : string) {
    if (c >= '0' && c <= '9')
      continue;
    if (c >= 'a' && c <= 'f')
      continue;
    if (c >= 'A' && c <= 'F')
      continue;
    return false;
  }
  return true;
}

bool CompareEntry(const UsbBlocklist::Entry& a, const UsbBlocklist::Entry& b) {
  return std::tie(a.vendor_id, a.product_id, a.max_version) <
         std::tie(b.vendor_id, b.product_id, b.max_version);
}

// Returns true if an entry in (begin, end] matches the vendor and product IDs
// of |entry| and has a device version greater than or equal to |entry|.
template <class Iterator>
bool EntryMatches(Iterator begin,
                  Iterator end,
                  const UsbBlocklist::Entry& entry) {
  auto it = std::lower_bound(begin, end, entry, CompareEntry);
  return it != end && it->vendor_id == entry.vendor_id &&
         it->product_id == entry.product_id;
}

// This list must be sorted according to CompareEntry.
const UsbBlocklist::Entry kStaticEntries[] = {
    // Yubikey NEO - OTP and CCID
    {0x1050, 0x0111, kMaxVersion},
    // Yubikey NEO - CCID only
    {0x1050, 0x0112, kMaxVersion},
    // Yubikey NEO - U2F and CCID
    {0x1050, 0x0115, kMaxVersion},
    // Yubikey NEO - OTP, U2F and CCID
    {0x1050, 0x0116, kMaxVersion},
    // Google Gnubby (WinUSB firmware)
    {0x1050, 0x0211, kMaxVersion},
    // Yubikey 4 - CCID only
    {0x1050, 0x0404, kMaxVersion},
    // Yubikey 4 - OTP and CCID
    {0x1050, 0x0405, kMaxVersion},
    // Yubikey 4 - U2F and CCID
    {0x1050, 0x0406, kMaxVersion},
    // Yubikey 4 - OTP, U2F and CCID
    {0x1050, 0x0407, kMaxVersion},
};

}  // namespace

UsbBlocklist::Entry::Entry(uint16_t vendor_id,
                           uint16_t product_id,
                           uint16_t max_version)
    : vendor_id(vendor_id), product_id(product_id), max_version(max_version) {}

UsbBlocklist::~UsbBlocklist() {}

// static
UsbBlocklist& UsbBlocklist::Get() {
  return g_singleton.Get();
}

bool UsbBlocklist::IsExcluded(const Entry& entry) const {
  return EntryMatches(std::begin(kStaticEntries), std::end(kStaticEntries),
                      entry) ||
         EntryMatches(dynamic_entries_.begin(), dynamic_entries_.end(), entry);
}

bool UsbBlocklist::IsExcluded(
    const scoped_refptr<const device::UsbDevice>& device) const {
  return IsExcluded(Entry(device->vendor_id(), device->product_id(),
                          device->device_version()));
}

void UsbBlocklist::ResetToDefaultValuesForTest() {
  dynamic_entries_.clear();
  PopulateWithServerProvidedValues();
}

UsbBlocklist::UsbBlocklist() {
  DCHECK(std::is_sorted(std::begin(kStaticEntries), std::end(kStaticEntries),
                        CompareEntry));
  PopulateWithServerProvidedValues();
}

void UsbBlocklist::PopulateWithServerProvidedValues() {
  std::string blocklist_string = variations::GetVariationParamValue(
      "WebUSBBlocklist", "blocklist_additions");

  for (const auto& entry :
       base::SplitStringPiece(blocklist_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<base::StringPiece> components = base::SplitStringPiece(
        entry, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (components.size() != 3 || !IsHexComponent(components[0]) ||
        !IsHexComponent(components[1]) || !IsHexComponent(components[2])) {
      continue;
    }

    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t max_version;
    if (!base::HexStringToUInt(components[0], &vendor_id) ||
        !base::HexStringToUInt(components[1], &product_id) ||
        !base::HexStringToUInt(components[2], &max_version)) {
      continue;
    }

    dynamic_entries_.emplace_back(vendor_id, product_id, max_version);
  }

  std::sort(dynamic_entries_.begin(), dynamic_entries_.end(), CompareEntry);
}
