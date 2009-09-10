// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/character_set_converters.h"

#include <string>

using std::string;

namespace browser_sync {

void PathStringToUTF8(const PathChar* wide, int size,
                      std::string* output_string) {
  CHECK(output_string);
  output_string->clear();
  AppendPathStringToUTF8(wide, size, output_string);
}

bool UTF8ToPathString(const char* utf8, size_t size,
                      PathString* output_string) {
  CHECK(output_string);
  output_string->clear();
  return AppendUTF8ToPathString(utf8, size, output_string);
};

ToUTF8::ToUTF8(const PathChar* wide, size_t size) {
  PathStringToUTF8(wide, size, &result_);
}

ToUTF8::ToUTF8(const PathString& wide) {
  PathStringToUTF8(wide.data(), wide.length(), &result_);
}

ToUTF8::ToUTF8(const PathChar* wide) {
  PathStringToUTF8(wide, PathLen(wide), &result_);
}

ToPathString::ToPathString(const char* utf8, size_t size) {
  good_ = UTF8ToPathString(utf8, size, &result_);
  good_checked_ = false;
}

ToPathString::ToPathString(const std::string& utf8) {
  good_ = UTF8ToPathString(utf8.data(), utf8.length(), &result_);
  good_checked_ = false;
}

ToPathString::ToPathString(const char* utf8) {
  good_ = UTF8ToPathString(utf8, strlen(utf8), &result_);
  good_checked_ = false;
}

}  // namespace browser_sync
