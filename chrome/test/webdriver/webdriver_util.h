// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_UTIL_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_UTIL_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/test/automation/value_conversion_traits.h"

class FilePath;

namespace base {
class Value;
}

namespace webdriver {

// Generates a random, 32-character hexidecimal ID.
std::string GenerateRandomID();

// Returns the equivalent JSON string for the given value.
std::string JsonStringify(const base::Value* value);

#if defined(OS_MACOSX)
// Gets the paths to the user and local application directory.
void GetApplicationDirs(std::vector<FilePath>* app_dirs);
#endif

// Parses a given value.
class ValueParser {
 public:
  virtual ~ValueParser();
  virtual bool Parse(base::Value* value) const = 0;

 protected:
  ValueParser();

 private:
  DISALLOW_COPY_AND_ASSIGN(ValueParser);
};

// Define a special type and constant that allows users to skip parsing a value.
// Useful when wanting to skip parsing for one value out of many in a list.
enum SkipParsing { };
extern SkipParsing* kSkipParsing;

// Parses a given value using the |ValueConversionTraits| to the appropriate
// type. This assumes that a direct conversion can be performed without
// pulling the value out of a dictionary or list.
template <typename T>
class DirectValueParser : public ValueParser {
 public:
  explicit DirectValueParser(T* t) : t_(t) { }

  virtual ~DirectValueParser() { }

  virtual bool Parse(base::Value* value) const OVERRIDE {
    return ValueConversionTraits<T>::SetFromValue(value, t_);
  }

 private:
  T* t_;
  DISALLOW_COPY_AND_ASSIGN(DirectValueParser);
};

// Convenience function for creating a DirectValueParser.
template <typename T>
DirectValueParser<T>* CreateDirectValueParser(T* t) {
  return new DirectValueParser<T>(t);
}

}  // namespace webdriver

// Value conversion traits for SkipParsing, which just return true.
template <>
struct ValueConversionTraits<webdriver::SkipParsing> {
  static bool SetFromValue(const base::Value* value,
                           const webdriver::SkipParsing* t);
  static bool CanConvert(const base::Value* value);
};

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_UTIL_H_
