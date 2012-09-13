// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_

#include "build/build_config.h"

namespace chrome {

// Prefix constants used in device unique id.
extern const char kFSUniqueIdPrefix[];
extern const char kVendorModelSerialPrefix[];

#if defined(OS_CHROMEOS)
extern const char kVendorModelVolumeStoragePrefix[];
#endif

// Delimiter constants used in device label and unique id.
extern const char kNonSpaceDelim[];
extern const char kSpaceDelim[];

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_
