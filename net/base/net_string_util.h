// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_STRING_UTIL_H__
#define NET_BASE_NET_STRING_UTIL_H__

#include <string>

#include "base/strings/string16.h"

// String conversion functions.  By default, they're implemented with ICU, but
// when building with USE_ICU_ALTERNATIVES, they use platform functions instead.

namespace net {

// Converts |text| using |charset| to UTF-8, and writes it to |output|.
// On failure, returns false and |output| is cleared.
bool ConvertToUtf8(const std::string& text, const char* charset,
                   std::string* output);

// Converts |text| using |charset| to UTF-8, normalizes the result, and writes
// it to |output|.  On failure, returns false and |output| is cleared.
bool ConvertToUtf8AndNormalize(const std::string& text, const char* charset,
                               std::string* output);

// Converts |text| encoded in Latin-1 to UTF-8, normalizes the result, and
// writes it to |output|.  On failure, returns false and |output| is cleared.
bool ConvertLatin1ToUtf8AndNormalize(const std::string& text,
                                     std::string* output);

// Converts |text| using |charset| to UTF-16, and writes it to |output|.
// On failure, returns false and |output| is cleared.
bool ConvertToUTF16(const std::string& text, const char* charset,
                    base::string16* output);

// Converts |text| encoded in Latin-1 to UTF-16, and writes it to |output|.
// On failure, returns false and |output| is cleared.
bool ConvertLatin1ToUTF16(const std::string& text, base::string16* output);

}  // namespace net

#endif  // NET_BASE_NET_STRING_UTIL_H__
