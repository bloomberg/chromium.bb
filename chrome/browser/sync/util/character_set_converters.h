// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_CHARACTER_SET_CONVERTERS_H_
#define CHROME_BROWSER_SYNC_UTIL_CHARACTER_SET_CONVERTERS_H_

// A pair of classes to convert UTF8 <-> UCS2 character strings.
//
// Note that the current implementation is limited to UCS2, whereas the
// interface is agnostic to the wide encoding used.
//
// Also note that UCS2 is different from UTF-16, in that UTF-16 can encode all
// the code points in the Unicode character set by multi-character encodings,
// while UCS2 is limited to encoding < 2^16 code points.
//
// It appears that Windows support UTF-16, which means we have to be careful
// what we feed this class.
//
// Usage:
//   string utf8;
//   CHECK(browser_sync::Append(wide_string, &utf8));
//   PathString bob;
//   CHECK(browser_sync::Append(utf8, &bob));
//   PathString fred = bob;

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string16.h"
#include "chrome/browser/sync/util/sync_types.h"

// Need to cast literals (Linux, OSX)
#define STRING16_UGLY_DOUBLE_DEFINE_HACK(s) \
  reinterpret_cast<const char16*>(L##s)
#define STRING16(s) STRING16_UGLY_DOUBLE_DEFINE_HACK(s)

using std::string;

namespace browser_sync {

// These 2 classes are deprecated.  Instead, prefer the Append() functions.

// A class to convert wide -> UTF8.
class ToUTF8 {
 public:
  explicit ToUTF8(const PathChar* wide);
  ToUTF8(const PathChar* wide, PathString::size_type size);
  explicit ToUTF8(const PathString& wide);

  // cast operators
  operator const std::string&() const;
  operator const char*() const;

  // accessors
  const std::string& get_string() const;
  const char* data() const;
  std::string::size_type byte_length() const;

 private:
  std::string result_;
};

// A class to convert UTF8 -> wide.
class ToPathString {
 public:
  explicit ToPathString(const char*);
  ToPathString(const char*, size_t size);
  explicit ToPathString(const std::string&);

  // true iff UTF-8 to wide conversion succeeded in constructor.
  bool good() {
    good_checked_ = true;
    return good_;
  }

  // It's invalid to invoke the accessors or the cast operators unless the
  // string is good and good() has been invoked at least once.

  // Implicit casts to const PathString& and const PathChar*
  operator const PathString&() const;
  operator const PathChar*() const;

  // Accessors
  const PathString& get_string16() const;
  const PathChar* data() const;
  PathString::size_type length() const;

 private:
  PathString result_;

  // Conversion succeeded.
  bool good_;
  // good() has been invoked at least once.
  bool good_checked_;
};

// Converts the UCS2 string "wide" to UTF8 encoding and stores the result in
// output_string.
void PathStringToUTF8(const PathChar* wide, int size,
                      std::string* output_string);

// Converts UCS2 string wide to UTF8 encoding and appends the result to
// output_string.
void AppendPathStringToUTF8(const PathChar* wide, int size,
                            std::string* output_string);

// Converts the UTF8 encoded string "utf8" to UCS16 and stores the result in
// output_string.
//
// Returns true iff conversion was successful, false otherwise.
bool UTF8ToPathString(const char* utf8, size_t size,
                      PathString* output_string);

// Converts the UTF8 encoded string "utf8" to UCS2 and appends the result in
// output_string.
//
// Returns true iff conversion was successful, false otherwise.
bool AppendUTF8ToPathString(const char* utf8, size_t size,
                            PathString* output_string);

// Converts the UTF8 encoded string "utf8" to UCS2 and appends the result in
// output_string.
//
// @returns true iff conversion was successful, false otherwise.
inline bool AppendUTF8ToPathString(const std::string& utf8,
                                 PathString* output_string) {
  return AppendUTF8ToPathString(utf8.data(), utf8.length(), output_string);
}

// Converts UCS2 string wide to UTF8 encoding and appends the result to
// output_string.
inline void AppendPathStringToUTF8(const PathString& wide,
                                   std::string* output_string) {
  return AppendPathStringToUTF8(wide.data(), wide.length(), output_string);
}


inline bool Append(const PathChar* wide, int size,
                   std::string* output_string) {
  AppendPathStringToUTF8(wide, size, output_string);
  return true;
}

inline bool Append(const PathChar* wide, std::string* output_string) {
  AppendPathStringToUTF8(wide, PathLen(wide), output_string);
  return true;
}

inline bool Append(const std::string& utf8, PathString* output_string) {
  return AppendUTF8ToPathString(utf8.data(), utf8.length(), output_string);
}

#if !PATHSTRING_IS_STD_STRING
inline bool Append(const char* utf8, size_t size, PathString* output_string) {
  return AppendUTF8ToPathString(utf8, size, output_string);
}

inline bool Append(const char* s, int size, std::string* output_string) {
  output_string->append(s, size);
  return true;
}

inline bool Append(const char* utf8, PathString* output_string) {
  return AppendUTF8ToPathString(utf8, strlen(utf8), output_string);
}

inline bool Append(const char* s,  std::string* output_string) {
  output_string->append(s);
  return true;
}

inline bool Append(const PathString& wide, std::string* output_string) {
  return Append(wide.data(), wide.length(), output_string);
}

inline bool Append(const std::string& s, std::string* output_string) {
  return Append(s.data(), s.length(), output_string);
}
#endif

inline ToUTF8::operator const std::string&() const {
  return result_;
}

inline ToUTF8::operator const char*() const {
  return result_.c_str();
}

inline const std::string& ToUTF8::get_string() const {
  return result_;
}

inline const char* ToUTF8::data() const {
  return result_.data();
}

inline std::string::size_type ToUTF8::byte_length() const {
  return result_.size();
}

inline ToPathString::operator const PathString&() const {
  DCHECK(good_ && good_checked_);
  return result_;
}

inline ToPathString::operator const PathChar*() const {
  DCHECK(good_ && good_checked_);
  return result_.c_str();
}

inline const PathString& ToPathString::get_string16() const {
  DCHECK(good_ && good_checked_);
  return result_;
}

inline const PathChar* ToPathString::data() const {
  DCHECK(good_ && good_checked_);
  return result_.data();
}

inline PathString::size_type ToPathString::length() const {
  DCHECK(good_ && good_checked_);
  return result_.length();
}

void TrimPathStringToValidCharacter(PathString* string);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_CHARACTER_SET_CONVERTERS_H_
