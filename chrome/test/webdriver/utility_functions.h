// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_UTILITY_FUNCTIONS_H_
#define CHROME_TEST_WEBDRIVER_UTILITY_FUNCTIONS_H_

#include <string>

#include "base/values.h"

namespace webdriver {

// Converts a value type to a string for logging.
std::string print_valuetype(Value::ValueType e);

// Checks that a value has the expected type.
void CheckValueType(const Value::ValueType expected, const Value* const actual);

// Attempts to parse a |json| string into a valid dictionary. If the parse
// operation fails, the offending |error| will be reported to the user and the
// function will return false. The caller is responsible for the allocated
// memory in |dict|.
bool ParseJSONDictionary(const std::string& json, DictionaryValue** dict,
                         std::string* error);

// Generates a random, 32-character hexidecimal ID.
std::string GenerateRandomID();

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_UTILITY_FUNCTIONS_H_
