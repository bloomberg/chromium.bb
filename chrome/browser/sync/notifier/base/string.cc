// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef OS_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <float.h>
#include <string.h>

#include "base/format_macros.h"
#include "base/string_util.h"
#include "chrome/browser/sync/notifier/base/string.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/stringencode.h"

using base::snprintf;

namespace notifier {

std::string HtmlEncode(const std::string& src) {
  size_t max_length = src.length() * 6 + 1;
  std::string dest;
  dest.resize(max_length);
  size_t new_size = talk_base::html_encode(&dest[0], max_length,
                                           src.data(), src.length());
  dest.resize(new_size);
  return dest;
}

std::string HtmlDecode(const std::string& src) {
  size_t max_length = src.length() + 1;
  std::string dest;
  dest.resize(max_length);
  size_t new_size = talk_base::html_decode(&dest[0], max_length,
                                           src.data(), src.length());
  dest.resize(new_size);
  return dest;
}

std::string UrlEncode(const std::string& src) {
  size_t max_length = src.length() * 6 + 1;
  std::string dest;
  dest.resize(max_length);
  size_t new_size = talk_base::url_encode(&dest[0], max_length,
                                          src.data(), src.length());
  dest.resize(new_size);
  return dest;
}

std::string UrlDecode(const std::string& src) {
  size_t max_length = src.length() + 1;
  std::string dest;
  dest.resize(max_length);
  size_t new_size = talk_base::url_decode(&dest[0], max_length,
                                          src.data(), src.length());
  dest.resize(new_size);
  return dest;
}

int CharToHexValue(char hex) {
  if (hex >= '0' && hex <= '9') {
    return hex - '0';
  } else if (hex >= 'A' && hex <= 'F') {
    return hex - 'A' + 10;
  } else if (hex >= 'a' && hex <= 'f') {
    return hex - 'a' + 10;
  } else {
    return -1;
  }
}

// Template function to convert a string to an int/int64
// If strict is true, check for the validity and overflow
template<typename T>
bool ParseStringToIntTemplate(const char* str,
                              T* value,
                              bool strict,
                              T min_value) {
  ASSERT(str);
  ASSERT(value);

  // Skip spaces
  while (*str == ' ') {
    ++str;
  }

  // Process sign
  int c = static_cast<int>(*str++);     // current char
  int possible_sign = c;                // save sign indication
  if (c == '-' || c == '+') {
    c = static_cast<int>(*str++);
  }

  // Process numbers
  T total = 0;
  while (c && (c = CharToDigit(static_cast<char>(c))) != -1) {
    // Check for overflow
    if (strict && (total < min_value / 10 ||
                   (total == min_value / 10 &&
                    c > ((-(min_value + 10)) % 10)))) {
      return false;
    }

    // Accumulate digit
    // Note that we accumulate in the negative direction so that we will not
    // blow away with the largest negative number
    total = 10 * total - c;

    // Get next char
    c = static_cast<int>(*str++);
  }

  // Fail if encountering non-numeric character
  if (strict && c == -1) {
    return false;
  }

  // Negate the number if needed
  if (possible_sign == '-') {
    *value = total;
  } else {
    // Check for overflow
    if (strict && total == min_value) {
      return false;
    }

    *value = -total;
  }

  return true;
}

// Convert a string to an int
// If strict is true, check for the validity and overflow
bool ParseStringToInt(const char* str, int* value, bool strict) {
  return ParseStringToIntTemplate<int>(str, value, strict, kint32min);
}

// Convert a string to an int
// This version does not check for the validity and overflow
int StringToInt(const char* str) {
  int value = 0;
  ParseStringToInt(str, &value, false);
  return value;
}

// Convert a string to an unsigned int.
// If strict is true, check for the validity and overflow
bool ParseStringToUint(const char* str, uint32* value, bool strict) {
  ASSERT(str);
  ASSERT(value);

  int64 int64_value;
  if (!ParseStringToInt64(str, &int64_value, strict)) {
    return false;
  }
  if (int64_value < 0 || int64_value > kuint32max) {
    return false;
  }

  *value = static_cast<uint32>(int64_value);
  return true;
}

// Convert a string to an int
// This version does not check for the validity and overflow
uint32 StringToUint(const char* str) {
  uint32 value = 0;
  ParseStringToUint(str, &value, false);
  return value;
}

// Convert a string to an int64
// If strict is true, check for the validity and overflow
bool ParseStringToInt64(const char* str, int64* value, bool strict) {
  return ParseStringToIntTemplate<int64>(str, value, strict, kint64min);
}

// Convert a string to an int64
// This version does not check for the validity and overflow
int64 StringToInt64(const char* str) {
  int64 value = 0;
  ParseStringToInt64(str, &value, false);
  return value;
}

// Convert a string to a double
// If strict is true, check for the validity and overflow
bool ParseStringToDouble(const char* str, double* value, bool strict) {
  ASSERT(str);
  ASSERT(value);

  // Skip spaces
  while (*str == ' ') {
    ++str;
  }

  // Process sign
  int c = static_cast<int>(*str++);     // current char
  int sign = c;                         // save sign indication
  if (c == '-' || c == '+') {
    c = static_cast<int>(*str++);
  }

  // Process numbers before "."
  double total = 0.0;
  while (c && (c != '.') && (c = CharToDigit(static_cast<char>(c))) != -1) {
    // Check for overflow
    if (strict && total >= DBL_MAX / 10) {
      return false;
    }

    // Accumulate digit
    total = 10.0 * total + c;

    // Get next char
    c = static_cast<int>(*str++);
  }

  // Process "."
  if (c == '.') {
    c = static_cast<int>(*str++);
  } else {
    // Fail if encountering non-numeric character
    if (strict && c == -1) {
      return false;
    }
  }

  // Process numbers after "."
  double power = 1.0;
  while ((c = CharToDigit(static_cast<char>(c))) != -1) {
    // Check for overflow
    if (strict && total >= DBL_MAX / 10) {
      return false;
    }

    // Accumulate digit
    total = 10.0 * total + c;
    power *= 10.0;

    // Get next char
    c = static_cast<int>(*str++);
  }

  // Get the final number
  *value = total / power;
  if (sign == '-') {
    *value = -(*value);
  }

  return true;
}

// Convert a string to a double
// This version does not check for the validity and overflow
double StringToDouble(const char* str) {
  double value = 0;
  ParseStringToDouble(str, &value, false);
  return value;
}

// Convert a float to a string
std::string FloatToString(float f) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%f", f);
  return std::string(buf);
}

std::string DoubleToString(double d) {
  char buf[160];
  snprintf(buf, sizeof(buf), "%.17g", d);
  return std::string(buf);
}

std::string UIntToString(uint32 i) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%lu", i);
  return std::string(buf);
}

// Convert an int to a string
std::string IntToString(int i) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%d", i);
  return std::string(buf);
}

// Convert an int64 to a string
std::string Int64ToString(int64 i64) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%" PRId64 "d", i64);
  return std::string(buf);
}

std::string UInt64ToString(uint64 i64) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%" PRId64 "u", i64);
  return std::string(buf);
}

std::string Int64ToHexString(int64 i64) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%" PRId64 "x", i64);
  return std::string(buf);
}

// Parse a single "delim" delimited string from "*source"
// Modify *source to point after the delimiter.
// If no delimiter is present after the string, set *source to NULL.
//
// Mainly a stringified wrapper around strpbrk()
std::string SplitOneStringToken(const char** source, const char* delim) {
  ASSERT(source);
  ASSERT(delim);

  if (!*source) {
    return std::string();
  }
  const char* begin = *source;
  *source = strpbrk(*source, delim);
  if (*source) {
    return std::string(begin, (*source)++);
  } else {
    return std::string(begin);
  }
}

std::string LowerWithUnderToPascalCase(const char* lower_with_under) {
  ASSERT(lower_with_under);

  std::string pascal_case;
  bool make_upper = true;
  for (; *lower_with_under != '\0'; lower_with_under++) {
    char current_char = *lower_with_under;
    if (current_char == '_') {
      ASSERT(!make_upper);
      make_upper = true;
      continue;
    }
    if (make_upper) {
      current_char = toupper(current_char);
      make_upper = false;
    }
    pascal_case.append(1, current_char);
  }
  return pascal_case;
}

std::string PascalCaseToLowerWithUnder(const char* pascal_case) {
  ASSERT(pascal_case);

  std::string lower_with_under;
  bool previous_was_upper = true;
  for(; *pascal_case != '\0'; pascal_case++) {
    char current_char = *pascal_case;
    if (isupper(current_char)) {
      // DNSName should be dns_name
      if ((islower(pascal_case[1]) && !lower_with_under.empty()) ||
          !previous_was_upper) {
        lower_with_under.append(1, '_');
      }
      current_char = tolower(current_char);
    } else if (previous_was_upper) {
      previous_was_upper = false;
    }
    lower_with_under.append(1, current_char);
  }
  return lower_with_under;
}
void StringReplace(std::string* s,
                   const char* old_sub,
                   const char* new_sub,
                   bool replace_all) {
  ASSERT(s);

  // If old_sub is empty, nothing to do
  if (!old_sub || !*old_sub) {
    return;
  }

  int old_sub_size = strlen(old_sub);
  std::string res;
  std::string::size_type start_pos = 0;

  do {
    std::string::size_type pos = s->find(old_sub, start_pos);
    if (pos == std::string::npos) {
      break;
    }
    res.append(*s, start_pos, pos - start_pos);
    res.append(new_sub);
    start_pos = pos + old_sub_size;  // start searching again after the "old"
  } while (replace_all);
  res.append(*s, start_pos, s->length() - start_pos);

  *s = res;
}

}  // namespace notifier
