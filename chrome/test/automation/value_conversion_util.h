// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_VALUE_CONVERSION_UTIL_H_
#define CHROME_TEST_AUTOMATION_VALUE_CONVERSION_UTIL_H_
#pragma once

#include <string>

#include "base/values.h"
#include "chrome/test/automation/value_conversion_traits.h"

// Creates a value from the given parameter. Will not return NULL.
template <typename T>
base::Value* CreateValueFrom(const T& t) {
  return ValueConversionTraits<T>::CreateValueFrom(t);
}

// Sets |t| from the given value. Returns true on success. |t| will not be
// modified unless successful.
template <typename T>
bool SetFromValue(const base::Value* value, T* t) {
  return ValueConversionTraits<T>::SetFromValue(value, t);
}

// Creates a list value containing the converted value from the the given
// parameter.
template <typename T>
base::ListValue* CreateListValueFrom(const T& t) {
  base::ListValue* list = new base::ListValue();
  list->Append(CreateValueFrom(t));
  return list;
}

// Same as above, but with more arguments.
template <typename T1, typename T2>
base::ListValue* CreateListValueFrom(const T1& t1, const T2& t2) {
  base::ListValue* list = new base::ListValue();
  list->Append(CreateValueFrom(t1));
  list->Append(CreateValueFrom(t2));
  return list;
}

// Same as above, but with more arguments.
template <typename T1, typename T2, typename T3>
base::ListValue* CreateListValueFrom(const T1& t1, const T2& t2, const T3& t3) {
  base::ListValue* list = new base::ListValue();
  list->Append(CreateValueFrom(t1));
  list->Append(CreateValueFrom(t2));
  list->Append(CreateValueFrom(t3));
  return list;
}

// Sets |t| from the first element in the given list. Returns true on success.
// |t| will not be modified unless successful.
template <typename T>
bool SetFromListValue(const base::ListValue* list, T* t) {
  if (list->GetSize() != 1)
    return false;

  if (!ValueConversionTraits<T>::CanConvert(*list->begin()))
    return false;

  CHECK(SetFromValue(*list->begin(), t));
  return true;
}

// Same as above, but with more arguments.
template <typename T1, typename T2>
bool SetFromListValue(const base::ListValue* list, T1* t1, T2* t2) {
  if (list->GetSize() != 2)
    return false;

  if (!ValueConversionTraits<T1>::CanConvert(*list->begin()))
    return false;
  if (!ValueConversionTraits<T2>::CanConvert(*(list->begin() + 1)))
    return false;

  CHECK(SetFromValue(*list->begin(), t1));
  CHECK(SetFromValue(*(list->begin() + 1), t2));
  return true;
}

// Same as above, but with more arguments.
template <typename T1, typename T2, typename T3>
bool SetFromListValue(const base::ListValue* list, T1* t1, T2* t2, T3* t3) {
  if (list->GetSize() != 3)
    return false;

  if (!ValueConversionTraits<T1>::CanConvert(*list->begin()))
    return false;
  if (!ValueConversionTraits<T2>::CanConvert(*(list->begin() + 1)))
    return false;
  if (!ValueConversionTraits<T3>::CanConvert(*(list->begin() + 2)))
    return false;

  CHECK(SetFromValue(*list->begin(), t1));
  CHECK(SetFromValue(*(list->begin() + 1), t2));
  CHECK(SetFromValue(*(list->begin() + 2), t3));
  return true;
}

#endif  // CHROME_TEST_AUTOMATION_VALUE_CONVERSION_UTIL_H_
