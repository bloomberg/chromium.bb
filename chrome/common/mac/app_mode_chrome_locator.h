// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_MODE_CHROME_LOCATOR_H_
#define CHROME_COMMON_MAC_APP_MODE_CHROME_LOCATOR_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/strings/string16.h"

@class NSString;

namespace base {
class FilePath;
}

namespace app_mode {

// Given a bundle id, return the path of the corresponding bundle.
// Returns true if the bundle was found, false otherwise.
bool FindBundleById(NSString* bundle_id, base::FilePath* out_bundle);

// Given the path to the Chrome bundle, read the following information:
// |executable_path| - Path to the Chrome executable.
// |raw_version_str| - Chrome version.
// |version_path| - |chrome_bundle|/Contents/Versions/|raw_version_str|/
// |framework_shlib_path| - Path to the chrome framework's shared library (not
//                          the framework directory).
// Returns true if all information read succesfuly, false otherwise.
bool GetChromeBundleInfo(const base::FilePath& chrome_bundle,
                         base::FilePath* executable_path,
                         base::string16* raw_version_str,
                         base::FilePath* version_path,
                         base::FilePath* framework_shlib_path);

}  // namespace app_mode

#endif  // CHROME_COMMON_MAC_APP_MODE_CHROME_LOCATOR_H_
