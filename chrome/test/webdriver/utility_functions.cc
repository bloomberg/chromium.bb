// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/utility_functions.h"

#include <string.h>
#include <wchar.h>
#include <algorithm>
#include <sstream>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "third_party/webdriver/atoms.h"

namespace webdriver {

std::string build_atom(const wchar_t* const atom[], const size_t& size) {
  const size_t len = size / sizeof(atom[0]);
  std::string ret = "";
  for (size_t i = 0; i < len; ++i) {
    if (atom[i] != NULL)
      ret.append(WideToUTF8(atom[i]));
  }
  return ret;
}

std::string print_valuetype(Value::ValueType e) {
  switch (e) {
    case Value::TYPE_NULL:
      return "NULL ";
    case Value::TYPE_BOOLEAN:
      return "BOOL";
    case Value::TYPE_INTEGER:
      return "INT";
    case Value::TYPE_DOUBLE:
      return "DOUBLE";
    case Value::TYPE_STRING:
      return "STRING";
    case Value::TYPE_BINARY:
      return "BIN";
    case Value::TYPE_DICTIONARY:
      return "DICT";
    case Value::TYPE_LIST:
      return "LIST";
    default:
      return "ERROR";
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

std::string GenerateRandomID() {
  uint64 msb = base::RandUint64();
  uint64 lsb = base::RandUint64();
  return base::StringPrintf("%016" PRIx64 "%016" PRIx64, msb, lsb);
}

}  // namespace webdriver
