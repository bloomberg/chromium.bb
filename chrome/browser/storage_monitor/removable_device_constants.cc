// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/removable_device_constants.h"

namespace chrome {

const char kFSUniqueIdPrefix[] = "UUID:";
const char kVendorModelSerialPrefix[] = "VendorModelSerial:";

#if defined(OS_LINUX)
const char kVendorModelVolumeStoragePrefix[] = "VendorModelVolumeStorage:";
#endif

#if defined(OS_WIN)
const char16 kWPDDevInterfaceGUID[] = L"{6ac27878-a6fa-4155-ba85-f98f491d4f33}";
#endif

const base::FilePath::CharType kDCIMDirectoryName[] = FILE_PATH_LITERAL("DCIM");

}  // namespace chrome
