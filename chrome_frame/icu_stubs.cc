// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/url_canon.h"

#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
// Avoid dependency on string_util_icu.cc (which pulls in icu).

std::string WideToAnsiDirect(const wchar_t* wide, size_t wide_len) {
  std::string ret;
  char* write = WriteInto(&ret, wide_len + 1);
  for (size_t i = 0; i < wide_len; ++i) {
    // We can only convert characters below 0x80 directly from wide to ansi.
    DCHECK(wide[i] <= 127) << "can't convert";
    write[i] = static_cast<char>(wide[i]);
  }

  write[wide_len] = '\0';

  return ret;
}

bool WideToUTF8(const wchar_t* wide, size_t wide_len, std::string* utf8) {
  DCHECK(utf8);

  // Add a cutoff. If it's all ASCII, convert it directly
  size_t i;
  for (i = 0; i < wide_len; ++i) {
    if (wide[i] > 127)
      break;
  }

  // If we made it to the end without breaking, then it's all ANSI, so do a
  // quick convert
  if (i == wide_len) {
    *utf8 = WideToAnsiDirect(wide, wide_len);
    return true;
  }

  // Figure out how long the string is
  int size = WideCharToMultiByte(CP_UTF8, 0, wide, wide_len + 1, NULL, 0, NULL,
                                 NULL);

  if (size > 0) {
    WideCharToMultiByte(CP_UTF8, 0, wide, wide_len + 1, WriteInto(utf8, size),
                        size, NULL, NULL);
  }

  return (size > 0);
}

std::string WideToUTF8(const std::wstring& wide) {
  std::string ret;
  if (!wide.empty()) {
    // Ignore the success flag of this call, it will do the best it can for
    // invalid input, which is what we want here.
    WideToUTF8(wide.data(), wide.length(), &ret);
  }
  return ret;
}

bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output) {
  DCHECK(output);

  if (src_len == 0) {
    output->clear();
    return true;
  }

  int wide_chars = MultiByteToWideChar(CP_UTF8, 0, src, src_len, NULL, 0);
  if (!wide_chars) {
    NOTREACHED();
    return false;
  }

  wide_chars++;  // make room for L'\0'
  // Note that WriteInto will fill the string with '\0', so in the case
  // where the input string is not \0 terminated, we will still be ensured
  // that the output string will be.
  if (!MultiByteToWideChar(CP_UTF8, 0, src, src_len,
                           WriteInto(output, wide_chars), wide_chars)) {
    NOTREACHED();
    output->clear();
    return false;
  }

  return true;
}

std::wstring UTF8ToWide(const base::StringPiece& utf8) {
  std::wstring ret;
  if (!utf8.empty())
    UTF8ToWide(utf8.data(), utf8.length(), &ret);
  return ret;
}

#ifdef WCHAR_T_IS_UTF16
string16 UTF8ToUTF16(const std::string& utf8) {
  std::wstring ret;
  if (!utf8.empty())
    UTF8ToWide(utf8.data(), utf8.length(), &ret);
  return ret;
}
#else
#error Need WCHAR_T_IS_UTF16
#endif

////////////////////////////////////////////////////////////////////////////////
// Replace ICU dependent functions in googleurl.
/*#define __UTF_H__
#include "third_party/icu38/public/common/unicode/utf16.h"
#define U_IS_SURROGATE(c) (((c)&0xfffff800)==0xd800)
extern const char16 kUnicodeReplacementCharacter;*/

namespace url_canon {

bool IDNToASCII(const char16* src, int src_len, CanonOutputW* output) {
  // We should only hit this when the user attempts to navigate
  // CF to an invalid URL.
  DLOG(WARNING) << __FUNCTION__ << " not implemented";
  return false;
}

bool ReadUTFChar(const char* str, int* begin, int length,
                 unsigned* code_point_out) {
  // We should only hit this when the user attempts to navigate
  // CF to an invalid URL.
  DLOG(WARNING) << __FUNCTION__ << " not implemented";

  // TODO(tommi): consider if we can use something like
  // http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
  return false;
}

bool ReadUTFChar(const char16* str, int* begin, int length,
                 unsigned* code_point) {
/*
  if (U16_IS_SURROGATE(str[*begin])) {
    if (!U16_IS_SURROGATE_LEAD(str[*begin]) || *begin + 1 >= length ||
        !U16_IS_TRAIL(str[*begin + 1])) {
      // Invalid surrogate pair.
      *code_point = kUnicodeReplacementCharacter;
      return false;
    } else {
      // Valid surrogate pair.
      *code_point = U16_GET_SUPPLEMENTARY(str[*begin], str[*begin + 1]);
      (*begin)++;
    }
  } else {
    // Not a surrogate, just one 16-bit word.
    *code_point = str[*begin];
  }

  if (U_IS_UNICODE_CHAR(*code_point))
    return true;

  // Invalid code point.
  *code_point = kUnicodeReplacementCharacter;
  return false;*/
  CHECK(false);
  return false;
}

}  // namespace url_canon
