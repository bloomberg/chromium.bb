// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_UTILS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_UTILS_H_

#include <string>

#if defined(OS_CHROMEOS)
#include <bluetooth/bluetooth.h>
#endif

namespace device {
namespace bluetooth_utils {

#if defined(OS_CHROMEOS)
// Converts a bluetooth address in the format "B0:D0:9C:0F:3A:2D" into a
// bdaddr_t struct.  Returns true on success, false on failure.  The contents
// of |out_address| are zeroed on failure.
// Note that the order is reversed upon conversion.  For example,
// "B0:D0:9C:0F:3A:2D" -> {"0x2d", "0x3a", "0x0f", "0x9c", "0xd0", "0xb0"}
bool str2ba(const std::string& in_address, bdaddr_t* out_address);
#endif

// Takes a 4, 8 or 36 character UUID, validates it and returns it in 36
// character format with all hex digits lower case.  If |uuid| is invalid, the
// empty string is returned.
//
// Valid inputs are:
//   XXXX
//   0xXXXX
//   XXXXXXXX
//   0xXXXXXXXX
//   XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
std::string CanonicalUuid(std::string uuid);

}  // namespace bluetooth_utils
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_UTILS_H_

