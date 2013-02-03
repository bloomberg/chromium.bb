// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_UTILS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_UTILS_H_

#include <string>

#include "base/basictypes.h"

namespace device {
namespace bluetooth_utils {

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

