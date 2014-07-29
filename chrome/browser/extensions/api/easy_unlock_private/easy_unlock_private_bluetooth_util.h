// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_BLUETOOTH_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_BLUETOOTH_UTIL_H_

#include <string>

#include "base/callback_forward.h"

namespace extensions {
namespace api {
namespace easy_unlock {

struct SeekDeviceResult {
  // Whether the connection to the device succeeded.
  bool success;

  // If the connection failed, an error message describing the failure.
  std::string error_message;
};
typedef base::Callback<void(const SeekDeviceResult& result)> SeekDeviceCallback;

// Connects to the SDP service on the Bluetooth device with the given
// |device_address|, if possible. Calls the |callback| with an indicator of
// success or an error message on failure.
void SeekBluetoothDeviceByAddress(const std::string& device_address,
                                  const SeekDeviceCallback& callback);

}  // namespace easy_unlock
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_BLUETOOTH_UTIL_H_
