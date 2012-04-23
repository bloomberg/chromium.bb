// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_UTILS_H_
#pragma once

#include <string>

#include <bluetooth/bluetooth.h>

namespace chromeos {
namespace bluetooth_utils {

// Converts a bluetooth address in the format "B0:D0:9C:0F:3A:2D" into a
// bdaddr_t struct.  Returns true on success, false on failure.  The contents
// of |out_address| are zeroed on failure.
bool str2ba(const std::string& in_address, bdaddr_t* out_address);

}  // namespace bluetooth_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_UTILS_H_

