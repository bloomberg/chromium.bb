// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_UTILS_MAC_H_
#define DEVICE_HID_HID_UTILS_MAC_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"

#include <IOKit/hid/IOHIDManager.h>

namespace device {

bool GetHidIntProperty(IOHIDDeviceRef device, CFStringRef key, int32_t* result);

bool GetHidStringProperty(IOHIDDeviceRef device,
                       CFStringRef key,
                       std::string* result);

}  // namespace device

#endif  // DEVICE_HID_HID_UTILS_MAC_H_
