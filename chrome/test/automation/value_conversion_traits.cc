// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/value_conversion_traits.h"

#include "base/values.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

Value* ValueConversionTraits<int>::CreateValueFrom(int t) {
  return Value::CreateIntegerValue(t);
}

bool ValueConversionTraits<int>::SetFromValue(const Value* value, int* t) {
  return value->GetAsInteger(t);
}

bool ValueConversionTraits<int>::CanConvert(const Value* value) {
  int t;
  return SetFromValue(value, &t);
}

Value* ValueConversionTraits<bool>::CreateValueFrom(bool t) {
  return Value::CreateBooleanValue(t);
}

bool ValueConversionTraits<bool>::SetFromValue(const Value* value, bool* t) {
  return value->GetAsBoolean(t);
}

bool ValueConversionTraits<bool>::CanConvert(const Value* value) {
  bool t;
  return SetFromValue(value, &t);
}

Value* ValueConversionTraits<std::string>::CreateValueFrom(
    const std::string& t) {
  return Value::CreateStringValue(t);
}

bool ValueConversionTraits<std::string>::SetFromValue(
    const Value* value, std::string* t) {
  return value->GetAsString(t);
}

bool ValueConversionTraits<std::string>::CanConvert(const Value* value) {
  std::string t;
  return SetFromValue(value, &t);
}

Value* ValueConversionTraits<Value*>::CreateValueFrom(const Value* t) {
  return t->DeepCopy();
}

bool ValueConversionTraits<Value*>::SetFromValue(
    const Value* value, Value** t) {
  *t = value->DeepCopy();
  return true;
}

bool ValueConversionTraits<Value*>::CanConvert(const Value* value) {
  return true;
}

Value* ValueConversionTraits<ListValue*>::CreateValueFrom(const ListValue* t) {
  return t->DeepCopy();
}

bool ValueConversionTraits<ListValue*>::SetFromValue(const Value* value,
                                                     ListValue** t) {
  ListValue* list = const_cast<Value*>(value)->AsList();
  if (!list)
    return false;
  *t = list->DeepCopy();
  return true;
}

bool ValueConversionTraits<ListValue*>::CanConvert(const Value* value) {
  return const_cast<Value*>(value)->AsList();
}

Value* ValueConversionTraits<DictionaryValue*>::CreateValueFrom(
    const DictionaryValue* t) {
  return t->DeepCopy();
}

bool ValueConversionTraits<DictionaryValue*>::SetFromValue(
    const Value* value, DictionaryValue** t) {
  if (!value->IsType(Value::TYPE_DICTIONARY))
    return false;
  *t = static_cast<const DictionaryValue*>(value)->DeepCopy();
  return true;
}

bool ValueConversionTraits<DictionaryValue*>::CanConvert(const Value* value) {
  return value->IsType(Value::TYPE_DICTIONARY);
}
