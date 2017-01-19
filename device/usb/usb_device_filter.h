// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_FILTER_H_
#define DEVICE_USB_USB_DEVICE_FILTER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"

namespace base {
class Value;
}

namespace device {

class UsbDevice;

struct UsbDeviceFilter {
  UsbDeviceFilter();
  UsbDeviceFilter(const UsbDeviceFilter& other);
  ~UsbDeviceFilter();

  bool Matches(scoped_refptr<UsbDevice> device) const;
  std::unique_ptr<base::Value> ToValue() const;

  static bool MatchesAny(scoped_refptr<UsbDevice> device,
                         const std::vector<UsbDeviceFilter>& filters);

  base::Optional<uint16_t> vendor_id;
  base::Optional<uint16_t> product_id;
  base::Optional<uint8_t> interface_class;
  base::Optional<uint8_t> interface_subclass;
  base::Optional<uint8_t> interface_protocol;
  base::Optional<std::string> serial_number;
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_FILTER_H_
