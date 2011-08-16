// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_VALUE_CONVERSION_TRAITS_H_
#define CHROME_TEST_AUTOMATION_VALUE_CONVERSION_TRAITS_H_
#pragma once

#include <string>

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

// ValueConversionTraits contains functions for converting between a base::Value
// and a particular type. See particular function descriptions below.
template <typename T>
struct ValueConversionTraits {
  // Create a value from |t|. Will not return NULL.
  // static base::Value* CreateValueFrom(const T& t);

  // Attempts to set |t| from the given value. Returns true on success.
  // |t| will not be modified unless successful.
  // static bool SetFromValue(const base::Value* value, T* t);

  // Returns whether |value| can be converted to type |T|.
  // If true, |SetFromValue| will succeed.
  // static bool CanConvert(const base::Value* value);
};

template <>
struct ValueConversionTraits<int> {
  static base::Value* CreateValueFrom(int t);
  static bool SetFromValue(const base::Value* value, int* t);
  static bool CanConvert(const base::Value* value);
};

template <>
struct ValueConversionTraits<bool> {
  static base::Value* CreateValueFrom(bool t);
  static bool SetFromValue(const base::Value* value, bool* t);
  static bool CanConvert(const base::Value* value);
};

template <>
struct ValueConversionTraits<std::string> {
  static base::Value* CreateValueFrom(const std::string& t);
  static bool SetFromValue(const base::Value* value, std::string* t);
  static bool CanConvert(const base::Value* value);
};

// The conversion will involve deep copying, not just casting.
template <>
struct ValueConversionTraits<base::Value*> {
  static base::Value* CreateValueFrom(const base::Value* t);
  static bool SetFromValue(const base::Value* value, base::Value** t);
  static bool CanConvert(const base::Value* value);
};

// The conversion will involve deep copying, not just casting.
template <>
struct ValueConversionTraits<base::ListValue*> {
  static base::Value* CreateValueFrom(const base::ListValue* t);
  static bool SetFromValue(const base::Value* value, base::ListValue** t);
  static bool CanConvert(const base::Value* value);
};

// The conversion will involve deep copying, not just casting.
template <>
struct ValueConversionTraits<base::DictionaryValue*> {
  static base::Value* CreateValueFrom(const base::DictionaryValue* t);
  static bool SetFromValue(const base::Value* value, base::DictionaryValue** t);
  static bool CanConvert(const base::Value* value);
};

#endif  // CHROME_TEST_AUTOMATION_VALUE_CONVERSION_TRAITS_H_
