// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_MODE_CHROME_LOCATOR_H_
#define CHROME_COMMON_MAC_APP_MODE_CHROME_LOCATOR_H_
#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include "base/string16.h"

@class NSString;

class FilePath;

namespace app_mode {

// Given a bundle id, return the path of the corresponding bundle.
// Returns true if the bundle was found, false otherwise.
bool FindBundleById(NSString* bundle_id, FilePath* out_bundle);

// Given the path to the Chrome bundle, read the following information:
// |raw_version_str| - Chrome version.
// |version_path| - |chrome_bundle|/Contents/Versions/|raw_version_str|/
// |framework_shlib_path| - Path to the chrome framework's shared library (not
//                          the framework directory).
// Returns true if all information read succesfuly, false otherwise.
bool GetChromeBundleInfo(const FilePath& chrome_bundle,
                         string16* raw_version_str,
                         FilePath* version_path,
                         FilePath* framework_shlib_path);

}  // namespace app_mode

#endif  // CHROME_COMMON_MAC_APP_MODE_CHROME_LOCATOR_H_
