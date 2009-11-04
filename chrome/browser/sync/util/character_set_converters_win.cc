// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/util/character_set_converters.h"

namespace browser_sync {

// Returns UTF8 string from the given FilePath.
std::string FilePathToUTF8(const FilePath& file_path) {
  return WideToUTF8(file_path.value());
}

// Returns FilePath from the given UTF8 string.
FilePath UTF8ToFilePath(const std::string& utf8) {
  return FilePath(UTF8ToWide(utf8));
}

}  // namespace browser_sync
