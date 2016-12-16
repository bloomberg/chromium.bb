// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_BLOCKLIST_H_
#define CHROME_BROWSER_USB_USB_BLOCKLIST_H_

#include <stdint.h>

#include <set>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"

namespace device {
class UsbDevice;
}

class UsbBlocklist final {
 public:
  // An entry in the blocklist. Represents a specific version of a device that
  // should not be accessible. These fields correspond to the idVendor,
  // idProduct and bcdDevice fields of the device's USB device descriptor.
  struct Entry {
    Entry(uint16_t vendor_id, uint16_t product_id, uint16_t version);

    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t version;
  };

  ~UsbBlocklist();

  // Returns a singleton instance of the blocklist.
  static UsbBlocklist& Get();

  // Adds a device to the blocklist to be excluded from access.
  void Exclude(const Entry&);

  // Adds a device to the blocklist by parsing a blocklist string and calling
  // Exclude(entry).
  //
  // The blocklist string must be a comma-separated list of
  // idVendor:idProduct:bcdDevice triples, where each member of the triple is a
  // 16-bit integer written as exactly 4 hexadecimal digits. The triples may
  // be separated by whitespace. Triple components are colon-separated and must
  // not have whitespace around the colon.
  //
  // Invalid entries in the comma-separated list will be ignored.
  //
  // Example:
  //   "1000:001C:0100, 1000:001D:0101, 123:ignored:0"
  void Exclude(base::StringPiece blocklist_string);

  // Returns if a device is excluded from access.
  bool IsExcluded(const Entry&);
  bool IsExcluded(scoped_refptr<const device::UsbDevice>);

  // Size of the blocklist.
  size_t size() { return blocklisted_devices_.size(); }

  // Reload the blocklist for testing purposes.
  void ResetToDefaultValuesForTest();

 private:
  // friend LazyInstance to permit access to private constructor.
  friend base::DefaultLazyInstanceTraits<UsbBlocklist>;

  UsbBlocklist();

  void PopulateWithDefaultValues();

  // Populates the blocklist with values obtained dynamically from a server,
  // able to be updated without shipping new executable versions.
  void PopulateWithServerProvidedValues();

  // Set of blocklist entries.
  std::set<Entry> blocklisted_devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbBlocklist);
};

#endif  // CHROME_BROWSER_USB_USB_BLOCKLIST_H_
