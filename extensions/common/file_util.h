// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FILE_UTIL_H_
#define EXTENSIONS_COMMON_FILE_UTIL_H_

class GURL;

namespace base {
class FilePath;
}

namespace extensions {
namespace file_util {

// Get a relative file path from a chrome-extension:// URL.
base::FilePath ExtensionURLToRelativeFilePath(const GURL& url);

// Get a full file path from a chrome-extension-resource:// URL, If the URL
// points a file outside of root, this function will return empty FilePath.
base::FilePath ExtensionResourceURLToFilePath(const GURL& url,
                                              const base::FilePath& root);

}  // namespace file_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FILE_UTIL_H_
