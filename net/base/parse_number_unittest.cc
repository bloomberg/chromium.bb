// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/parse_number.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(ParseNumberTest, IntValidInputs) {
  const struct {
    const char* input;
    int output;
  } kTests[] = {
      {"0", 0},     {"00000", 0}, {"003", 3}, {"003", 3}, {"1234566", 1234566},
      {"987", 987}, {"010", 10},
  };

  for (const auto& test : kTests) {
    int result;
    ASSERT_TRUE(ParseNonNegativeDecimalInt(test.input, &result))
        << "Failed to parse: " << test.input;
    EXPECT_EQ(result, test.output) << "Failed to parse: " << test.input;
  }
}

TEST(ParseNumberTest, IntInvalidInputs) {
  const char* kTests[] = {
      "",
      "-23",
      "+42",
      " 123",
      "123 ",
      "123\n",
      "0xFF",
      "0x11",
      "x11",
      "F11",
      "AF",
      "0AF",
      "0.0",
      "13.",
      "13,000",
      "13.000",
      "13/5",
      "9999999999999999999999999999999999999999999999999999999999999999",
      "Inf",
      "NaN",
      "null",
      "dog",
  };

  for (const auto& input : kTests) {
    int result = 0xDEAD;
    ASSERT_FALSE(ParseNonNegativeDecimalInt(input, &result))
        << "Succeeded to parse: " << input;
    EXPECT_EQ(0xDEAD, result) << "Modified output for failed parsing";
  }
}

TEST(ParseNumberTest, IntInvalidInputsContainsNul) {
  int result = 0xDEAD;
  ASSERT_FALSE(
      ParseNonNegativeDecimalInt(base::StringPiece("123\0", 4), &result));
  EXPECT_EQ(0xDEAD, result);
}

}  // namespace
}  // namespace net
