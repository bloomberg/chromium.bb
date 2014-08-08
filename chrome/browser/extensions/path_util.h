// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PATH_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_PATH_UTIL_H_

#include "base/files/file_path.h"

namespace extensions {
namespace path_util {

// Prettifies |source_path|, by replacing the user's home directory with "~"
// (if applicable).
// For OS X, prettifies |source_path| by localizing every component of the
// path. Additionally, if the path is inside the user's home directory, then
// replace the home directory component with "~".
//
// This function is tested in
// chrome/browser/extensions/extension_path_util_unittest.cc.
base::FilePath PrettifyPath(const base::FilePath& source_path);

}  // namespace path_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PATH_UTIL_H_
