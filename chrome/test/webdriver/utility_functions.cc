// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/utility_functions.h"

#include <string.h>
#include <wchar.h>
#include <algorithm>
#include <sstream>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"

#include "third_party/webdriver/atoms.h"

namespace webdriver {

std::wstring build_atom(const wchar_t* const atom[], const size_t& size) {
  const size_t len = size / sizeof(atom[0]);
  std::wstring ret = L"";
  for (size_t i = 0; i < len; ++i) {
    if (atom[i] != NULL)
      ret.append(std::wstring(atom[i]));
  }
  return ret;
}

std::wstring print_valuetype(Value::ValueType e) {
  switch (e) {
    case Value::TYPE_NULL:
      return L"NULL ";
    case Value::TYPE_BOOLEAN:
      return L"BOOL";
    case Value::TYPE_INTEGER:
      return L"INT";
    case Value::TYPE_DOUBLE:
      return L"DOUBLE";
    case Value::TYPE_STRING:
      return L"STRING";
    case Value::TYPE_BINARY:
      return L"BIN";
    case Value::TYPE_DICTIONARY:
      return L"DICT";
    case Value::TYPE_LIST:
      return L"LIST";
    default:
      return L"ERROR";
  }
}

void CheckValueType(const Value::ValueType expected,
                    const Value* const actual) {
  DCHECK(actual != NULL) << "Expected value to be non-NULL";
  DCHECK(expected == actual->GetType())
      << "Expected " << print_valuetype(expected)
      << ", but was " << print_valuetype(actual->GetType());
}

bool ParseJSONDictionary(const std::string& json, DictionaryValue** dict,
                         std::string* error) {
  int error_code = 0;
  Value* params =
    base::JSONReader::ReadAndReturnError(json, true, &error_code, error);
  if (error_code != 0) {
    VLOG(1) << "Could not parse JSON object, " << *error;
    if (params)
      delete params;
    return false;
  }

  if (!params || params->GetType() != Value::TYPE_DICTIONARY) {
    *error = "Data passed in URL must be of type dictionary.";
    VLOG(1) << "Invalid type to parse";
    if (params)
      delete params;
    return false;
  }

  *dict = static_cast<DictionaryValue*>(params);
  return true;
}

}  // namespace webdriver

