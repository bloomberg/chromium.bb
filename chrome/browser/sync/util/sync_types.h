// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_SYNC_TYPES_H_
#define CHROME_BROWSER_SYNC_UTIL_SYNC_TYPES_H_

#include <iosfwd>
#include <string>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "build/build_config.h"

#define PSTR(s) s
#define PSTR_CHAR char
typedef std::string PathString;

#if !defined(OS_WIN)
// Mac OS X typedef's BOOL to signed char, so we do that on Linux too.
typedef signed char BOOL;
typedef int32 LONG;
typedef uint32 DWORD;
typedef int64 LONGLONG;
typedef uint64 ULONGLONG;

#define MAX_PATH PATH_MAX
#if !defined(TRUE)
const BOOL TRUE = 1;
#endif
#if !defined(FALSE)
const BOOL FALSE = 0;
#endif
#endif

typedef PathString::value_type PathChar;

inline size_t CountBytes(const std::wstring& s) {
  return s.size() * sizeof(std::wstring::value_type);
}

inline size_t CountBytes(const std::string &s) {
  return s.size() * sizeof(std::string::value_type);
}

inline PathString IntToPathString(int digit) {
  std::string tmp = StringPrintf("%d", digit);
  return PathString(tmp.begin(), tmp.end());
}

const size_t kSyncProtocolMaxNameLengthBytes = 255;

#endif  // CHROME_BROWSER_SYNC_UTIL_SYNC_TYPES_H_
