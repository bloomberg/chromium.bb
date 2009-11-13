// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_CHARACTER_SET_CONVERTERS_H_
#define CHROME_BROWSER_SYNC_UTIL_CHARACTER_SET_CONVERTERS_H_

#include <string>

#include "base/file_path.h"
#include "chrome/browser/sync/util/sync_types.h"

// Need to cast literals (Linux, OSX)
#define STRING16_UGLY_DOUBLE_DEFINE_HACK(s) \
    reinterpret_cast<const char16*>(L##s)
#define STRING16(s) STRING16_UGLY_DOUBLE_DEFINE_HACK(s)

namespace browser_sync {

// Returns UTF8 string from the given FilePath.
std::string FilePathToUTF8(const FilePath& file_path);

// Returns FilePath from the given UTF8 string.
FilePath UTF8ToFilePath(const std::string& utf8);

void TrimPathStringToValidCharacter(std::string* string);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_CHARACTER_SET_CONVERTERS_H_
