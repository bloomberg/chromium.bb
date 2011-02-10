// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VALUES_TEST_UTIL_H_
#define CHROME_TEST_VALUES_TEST_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class DictionaryValue;
class ListValue;

namespace test {

// All the functions below expect that the value for the given key in
// the given dictionary equals the given expected value.

void ExpectBooleanValue(bool expected_value,
                        const DictionaryValue& value,
                        const std::string& key);

void ExpectDictionaryValue(const DictionaryValue& expected_value,
                           const DictionaryValue& value,
                           const std::string& key);

void ExpectIntegerValue(int expected_value,
                        const DictionaryValue& value,
                        const std::string& key);

void ExpectListValue(const ListValue& expected_value,
                     const DictionaryValue& value,
                     const std::string& key);

void ExpectStringValue(const std::string& expected_value,
                       const DictionaryValue& value,
                       const std::string& key);

}  // namespace test

#endif  // CHROME_TEST_VALUES_TEST_UTIL_H_
