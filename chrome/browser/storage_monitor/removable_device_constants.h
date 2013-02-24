// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_
#define CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_

#include "base/files/file_path.h"
#include "base/string16.h"
#include "build/build_config.h"

namespace chrome {

// Prefix constants used in device unique id.
extern const char kFSUniqueIdPrefix[];
extern const char kVendorModelSerialPrefix[];

#if defined(OS_LINUX)
extern const char kVendorModelVolumeStoragePrefix[];
#endif

#if defined(OS_WIN)
// Windows portable device interface GUID constant.
extern const char16 kWPDDevInterfaceGUID[];
#endif

extern const base::FilePath::CharType kDCIMDirectoryName[];

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_
