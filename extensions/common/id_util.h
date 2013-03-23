// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ID_UTIL_H_
#define EXTENSIONS_COMMON_ID_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

namespace extensions {
namespace id_util {

// The number of bytes in a legal id.
extern const size_t kIdSize;

// Generates an extension ID from arbitrary input. The same input string will
// always generate the same output ID.
std::string GenerateId(const std::string& input);

// Generate an ID for an extension in the given path.
// Used while developing extensions, before they have a key.
std::string GenerateIdForPath(const base::FilePath& path);

// Normalize the path for use by the extension. On Windows, this will make
// sure the drive letter is uppercase.
base::FilePath MaybeNormalizePath(const base::FilePath& path);

}  // namespace id_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_ID_UTIL_H_
