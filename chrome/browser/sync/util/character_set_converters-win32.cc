// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/character_set_converters.h"

#include <windows.h>

#include <string>

using std::string;

namespace browser_sync {

// Converts input_string to UTF8 and appends the result into to output_string.
void AppendPathStringToUTF8(const PathChar* wide, int size,
                            string* output_string) {
  CHECK(output_string);
  if (0 == size)
    return;

  int needed_space = ::WideCharToMultiByte(CP_UTF8, 0, wide, size, 0, 0, 0, 0);
  // TODO(sync): This should flag an error when we move to an api that can let
  // utf-16 -> utf-8 fail.
  CHECK(0 != needed_space);
  string::size_type current_size = output_string->size();
  output_string->resize(current_size + needed_space);
  CHECK(0 != ::WideCharToMultiByte(CP_UTF8, 0, wide, size,
      &(*output_string)[current_size], needed_space, 0, 0));
}

bool AppendUTF8ToPathString(const char* utf8, size_t size,
                            PathString* output_string) {
  CHECK(output_string);
  if (0 == size)
    return true;
  // TODO(sync): Do we want to force precomposed characters here?
  int needed_wide_chars = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
      utf8, size, 0, 0);
  if (0 == needed_wide_chars) {
    DWORD err = ::GetLastError();
    if (MB_ERR_INVALID_CHARS == err)
      return false;
    CHECK(0 == err);
  }
  PathString::size_type current_length = output_string->size();
  output_string->resize(current_length + needed_wide_chars);
  CHECK(0 != ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, size,
      &(*output_string)[current_length], needed_wide_chars));
  return true;
}

void TrimPathStringToValidCharacter(PathString* string) {
  CHECK(string);
  // Constants from http://en.wikipedia.org/wiki/UTF-16
  if (string->empty())
    return;
  if (0x0dc00 == (string->at(string->length() - 1) & 0x0fc00))
    string->resize(string->length() - 1);
}

}  // namespace browser_sync
