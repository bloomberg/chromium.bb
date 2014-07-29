// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_bluetooth_util.h"

#include "base/callback.h"

namespace extensions {
namespace api {
namespace easy_unlock {

#if !defined(OS_CHROMEOS)
namespace {

const char kApiUnavailable[] = "This API is not implemented on this platform.";

}  // namespace

void SeekBluetoothDeviceByAddress(const std::string& device_address,
                                  const SeekDeviceCallback& callback) {
  SeekDeviceResult result;
  result.success = false;
  result.error_message = kApiUnavailable;
  callback.Run(result);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace easy_unlock
}  // namespace api
}  // namespace extensions
