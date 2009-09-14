// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_STRING_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_STRING_H_

#ifdef COMPILER_MSVC
#include <xhash>
#elif defined(__GNUC__)
#include <ext/hash_map>
#endif

#include <ctype.h>
#include <string>

#include "chrome/browser/sync/notifier/base/fastalloc.h"
#include "talk/base/basictypes.h"

namespace notifier {

// Does html encoding of strings.
std::string HtmlEncode(const std::string& src);

// Does html decoding of strings.
std::string HtmlDecode(const std::string& src);

// Does utl encoding of strings.
std::string UrlEncode(const std::string& src);

// Does url decoding of strings.
std::string UrlDecode(const std::string& src);

// Convert a character to a digit. If the character is not a digit return -1
// (same as CRT).
inline int CharToDigit(char c) {
  return ((c) >= '0' && (c) <= '9' ? (c) - '0' : -1);
}

int CharToHexValue(char hex);

// ----------------------------------------------------------------------
// ParseStringToInt()
// ParseStringToUint()
// ParseStringToInt64()
// ParseStringToDouble()
//
// Convert a string to an int/int64/double.
// If strict is true, check for the validity and overflow.
// ----------------------------------------------------------------------

bool ParseStringToInt(const char* str, int* value, bool strict);

bool ParseStringToUint(const char* str, uint32* value, bool strict);

bool ParseStringToInt64(const char* str, int64* value, bool strict);

bool ParseStringToDouble(const char* str, double* value, bool strict);

// ----------------------------------------------------------------------
// StringToInt()
// StringToUint()
// StringToInt64()
// StringToDouble()
//
// Convert a string to an int/int64/double.
// Note that these functions do not check for the validity or overflow.
// ----------------------------------------------------------------------

int StringToInt(const char* str);

uint32 StringToUint(const char* str);

int64 StringToInt64(const char* str);

double StringToDouble(const char* str);

// ----------------------------------------------------------------------
// FloatToString()
// DoubleToString()
// IntToString()
// UIntToString()
// Int64ToString()
// UInt64ToString()
//
// Convert various types to their string representation.  These all do
// the obvious, trivial thing.
// ----------------------------------------------------------------------

std::string FloatToString(float f);
std::string DoubleToString(double d);

std::string IntToString(int i);
std::string UIntToString(uint32 i);

std::string Int64ToString(int64 i64);
std::string UInt64ToString(uint64 i64);

std::string Int64ToHexString(int64 i64);

// ----------------------------------------------------------------------
// StringStartsWith()
// StringEndsWith()
//
// Check if a string starts or ends with a pattern.
// ----------------------------------------------------------------------

inline bool StringStartsWith(const std::string& s, const char* p) {
  return s.find(p) == 0;
}

inline bool StringEndsWith(const std::string& s, const char* p) {
  return s.rfind(p) == (s.length() - strlen(p));
}

// ----------------------------------------------------------------------
// MakeStringEndWith()
//
// If the string does not end with a pattern, make it end with it.
// ----------------------------------------------------------------------

inline std::string MakeStringEndWith(const std::string& s, const char* p) {
  if (StringEndsWith(s, p)) {
    return s;
  } else {
    std::string ns(s);
    ns += p;
    return ns;
  }
}

// Convert a lower_case_string to LowerCaseString.
std::string LowerWithUnderToPascalCase(const char* lower_with_under);

// Convert a PascalCaseString to pascal_case_string.
std::string PascalCaseToLowerWithUnder(const char* pascal_case);

// ----------------------------------------------------------------------
// LowerString()
// LowerStringToBuf()
//
// Convert the characters in "s" to lowercase.
// Changes contents of "s".  LowerStringToBuf copies at most "n"
// characters (including the terminating '\0')  from "s" to another
// buffer.
// ----------------------------------------------------------------------

inline void LowerString(char* s) {
  for (; *s; ++s) {
    *s = tolower(*s);
  }
}

inline void LowerString(std::string* s) {
  std::string::iterator end = s->end();
  for (std::string::iterator i = s->begin(); i != end; ++i) {
    *i = tolower(*i);
  }
}

inline void LowerStringToBuf(const char* s, char* buf, int n) {
  for (int i = 0; i < n - 1; ++i) {
    char c = s[i];
    buf[i] = tolower(c);
    if (c == '\0') {
      return;
    }
  }
  buf[n - 1] = '\0';
}

// ----------------------------------------------------------------------
// UpperString()
// UpperStringToBuf()
//
// Convert the characters in "s" to uppercase.
// UpperString changes "s". UpperStringToBuf copies at most "n"
// characters (including the terminating '\0')  from "s" to another
// buffer.
// ----------------------------------------------------------------------

inline void UpperString(char* s) {
  for (; *s; ++s) {
    *s = toupper(*s);
  }
}

inline void UpperString(std::string* s) {
  for (std::string::iterator iter = s->begin(); iter != s->end(); ++iter) {
    *iter = toupper(*iter);
  }
}

inline void UpperStringToBuf(const char* s, char* buf, int n) {
  for (int i = 0; i < n - 1; ++i) {
    char c = s[i];
    buf[i] = toupper(c);
    if (c == '\0') {
      return;
    }
  }
  buf[n - 1] = '\0';
}

// ----------------------------------------------------------------------
// TrimStringLeft
//
// Removes any occurrences of the characters in 'remove' from the start
// of the string.  Returns the number of chars trimmed.
// ----------------------------------------------------------------------
inline int TrimStringLeft(std::string* s, const char* remove) {
  int i = 0;
  for (; i < static_cast<int>(s->size()) && strchr(remove, (*s)[i]); ++i);
  if (i > 0) s->erase(0, i);
  return i;
}

// ----------------------------------------------------------------------
// TrimStringRight
//
// Removes any occurrences of the characters in 'remove' from the end of
// the string.  Returns the number of chars trimmed.
// ----------------------------------------------------------------------
inline int TrimStringRight(std::string* s, const char* remove) {
  int size = static_cast<int>(s->size());
  int i = size;
  for (; i > 0 && strchr(remove, (*s)[i - 1]); --i);
  if (i < size) {
    s->erase(i);
  }
  return size - i;
}

// ----------------------------------------------------------------------
// TrimString
//
// Removes any occurrences of the characters in 'remove' from either end
// of the string.
// ----------------------------------------------------------------------
inline int TrimString(std::string* s, const char* remove) {
  return TrimStringRight(s, remove) + TrimStringLeft(s, remove);
}

// ----------------------------------------------------------------------
// StringReplace()
//
// Replace the "old" pattern with the "new" pattern in a string.  If
// replace_all is false, it only replaces the first instance of "old."
// ----------------------------------------------------------------------

void StringReplace(std::string* s,
                   const char* old_sub,
                   const char* new_sub,
                   bool replace_all);

inline size_t HashString(const std::string &value) {
#ifdef COMPILER_MSVC
  return stdext::hash_value(value);
#elif defined(__GNUC__)
  __gnu_cxx::hash<const char*> h;
  return h(value.c_str());
#else
  // Compile time error because we don't return a value.
#endif
}

// ----------------------------------------------------------------------
// SplitOneStringToken()
//
// Parse a single "delim" delimited string from "*source"
// Modify *source to point after the delimiter.
// If no delimiter is present after the string, set *source to NULL.
//
// If the start of *source is a delimiter, return an empty string.
// If *source is NULL, return an empty string.
// ----------------------------------------------------------------------
std::string SplitOneStringToken(const char** source, const char* delim);

//----------------------------------------------------------------------
// CharTraits provides wrappers with common function names for
// char/wchar_t specific CRT functions
//----------------------------------------------------------------------

template <class CharT> struct CharTraits {
};

template <>
struct CharTraits<char> {
  static inline size_t length(const char* s) {
    return strlen(s);
  }
  static inline bool copy(char* dst, size_t dst_size, const char* s) {
    if (s == NULL || dst == NULL)
      return false;
    else
      return copy_num(dst, dst_size, s, strlen(s));
  }
  static inline bool copy_num(char* dst, size_t dst_size, const char* s,
                              size_t s_len) {
    if (dst_size < (s_len + 1))
      return false;
    memcpy(dst, s, s_len);
    dst[s_len] = '\0';
    return true;
  }
};

template <>
struct CharTraits<wchar_t> {
  static inline size_t length(const wchar_t* s) {
    return wcslen(s);
  }
  static inline bool copy(wchar_t* dst, size_t dst_size, const wchar_t* s) {
    if (s == NULL || dst == NULL)
      return false;
    else
      return copy_num(dst, dst_size, s, wcslen(s));
  }
  static inline bool copy_num(wchar_t* dst, size_t dst_size, const wchar_t* s,
                              size_t s_len) {
    if (dst_size < (s_len + 1)) {
      return false;
    }
    memcpy(dst, s, s_len * sizeof(wchar_t));
    dst[s_len] = '\0';
    return true;
  }
};

//----------------------------------------------------------------------
// This class manages a fixed-size, null-terminated string buffer.  It
// is meant to be allocated on the stack, and it makes no use of the
// heap internally. In most cases you'll just want to use a
// std::(w)string, but when you need to avoid the heap, you can use this
// class instead.
//
// Methods are provided to read the null-terminated buffer and to append
// data to the buffer, and once the buffer fills-up, it simply discards
// any extra append calls.
//----------------------------------------------------------------------

template <class CharT, int MaxSize>
class FixedString {
 public:
  typedef CharTraits<CharT> char_traits;

  FixedString() : index_(0), truncated_(false) {
    buf_[0] = CharT(0);
  }

  ~FixedString() {
    memset(buf_, 0xCC, sizeof(buf_));
  }

  // Returns true if the Append ever failed.
  bool was_truncated() const { return truncated_; }

  // Returns the number of characters in the string, excluding the null
  // terminator.
  size_t size() const { return index_; }

  // Returns the null-terminated string.
  const CharT* get() const { return buf_; }
  CharT* get() { return buf_; }

  // Append an array of characters.  The operation is bounds checked, and if
  // there is insufficient room, then the was_truncated() flag is set to true.
  void Append(const CharT* s, size_t n) {
    if (char_traits::copy_num(buf_ + index_, arraysize(buf_) - index_, s, n)) {
      index_ += n;
    } else {
      truncated_ = true;
    }
  }

  // Append a null-terminated string.
  void Append(const CharT* s) {
    Append(s, char_traits::length(s));
  }

  // Append a single character.
  void Append(CharT c) {
    Append(&c, 1);
  }

 private:
  CharT buf_[MaxSize];
  size_t index_;
  bool truncated_;
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_STRING_H_
