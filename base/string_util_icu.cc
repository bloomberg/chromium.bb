// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"

#include <string.h>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "unicode/numfmt.h"
#include "unicode/ustring.h"

// Number formatting -----------------------------------------------------------

namespace {

struct NumberFormatSingletonTraits
    : public DefaultSingletonTraits<icu::NumberFormat> {
  static icu::NumberFormat* New() {
    UErrorCode status = U_ZERO_ERROR;
    icu::NumberFormat* formatter = icu::NumberFormat::createInstance(status);
    DCHECK(U_SUCCESS(status));
    return formatter;
  }
  // There's no ICU call to destroy a NumberFormat object other than
  // operator delete, so use the default Delete, which calls operator delete.
  // This can cause problems if a different allocator is used by this file than
  // by ICU.
};

}  // namespace

std::wstring FormatNumber(int64 number) {
  icu::NumberFormat* number_format =
      Singleton<icu::NumberFormat, NumberFormatSingletonTraits>::get();

  if (!number_format) {
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  icu::UnicodeString ustr;
  number_format->format(number, ustr);

#if defined(WCHAR_T_IS_UTF16)
  return std::wstring(ustr.getBuffer(),
                      static_cast<std::wstring::size_type>(ustr.length()));
#elif defined(WCHAR_T_IS_UTF32)
  wchar_t buffer[64];  // A int64 is less than 20 chars long,  so 64 chars
                       // leaves plenty of room for formating stuff.
  int length = 0;
  UErrorCode error = U_ZERO_ERROR;
  u_strToWCS(buffer, 64, &length, ustr.getBuffer(), ustr.length() , &error);
  if (U_FAILURE(error)) {
    NOTREACHED();
    // As a fallback, just return the raw number in a string.
    return StringPrintf(L"%lld", number);
  }
  return std::wstring(buffer, static_cast<std::wstring::size_type>(length));
#endif  // defined(WCHAR_T_IS_UTF32)
}

// Although this function isn't specific to ICU, we implemented it here so
// that chrome.exe won't pull it in.  Moving this function to string_util.cc
// causes chrome.exe to grow by 400k because of more ICU being pulled in.
TrimPositions TrimWhitespaceUTF8(const std::string& input,
                                 TrimPositions positions,
                                 std::string* output) {
  // This implementation is not so fast since it converts the text encoding
  // twice. Please feel free to file a bug if this function hurts the
  // performance of Chrome.
  DCHECK(IsStringUTF8(input));
  std::wstring input_wide = UTF8ToWide(input);
  std::wstring output_wide;
  TrimPositions result = TrimWhitespace(input_wide, positions, &output_wide);
  *output = WideToUTF8(output_wide);
  return result;
}
