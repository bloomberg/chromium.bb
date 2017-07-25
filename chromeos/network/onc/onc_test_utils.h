// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_TEST_UTILS_H_
#define CHROMEOS_NETWORK_ONC_ONC_TEST_UTILS_H_

#include <memory>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {
namespace onc {
namespace test_utils {

// Read the file at |filename| as a string. CHECKs if any error occurs.
std::string ReadTestData(const std::string& filename);

// Read a JSON dictionary from |filename| and return it as a
// DictionaryValue. CHECKs if any error occurs.
std::unique_ptr<base::DictionaryValue> ReadTestDictionary(
    const std::string& filename);

// Checks that the pointer |actual| is not NULL but points to a value that
// equals |expected|. The intended use case is:
// EXPECT_TRUE(test_utils::Equals(expected, actual));
::testing::AssertionResult Equals(const base::Value* expected,
                                  const base::Value* actual);

}  // namespace test_utils
}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_TEST_UTILS_H_
