// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MUSIC_MANAGER_PRIVATE_DEVICE_ID_H_
#define CHROME_BROWSER_EXTENSIONS_API_MUSIC_MANAGER_PRIVATE_DEVICE_ID_H_

#include <string>

namespace device_id {

// Return a "device" identifier with the following characteristics:
// 1. The id is shared across users of a device.
// 2. The id is resilient to device reboots.
// 3. There is *some* way for the identifier to be reset (e.g. it can *not* be
//    the MAC address of the device's network card).
// The specific implementation varies across platforms, but is currently
// fast enough that it can be run on the UI thread.
// The returned value is HMAC_SHA256(device_id, |salt|), so that the actual
// device identifier value is not exposed directly to the caller.
std::string GetDeviceID(const std::string& salt);

}  // namespace device_id

#endif  // CHROME_BROWSER_EXTENSIONS_API_MUSIC_MANAGER_PRIVATE_DEVICE_ID_H_
