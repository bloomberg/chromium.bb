// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "net/der/parse_values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace der {
namespace test {

TEST(ParseValuesTest, ParseBool) {
  uint8_t buf[] = {0xFF, 0x00};
  Input value(buf, 1);
  bool out;
  EXPECT_TRUE(ParseBool(value, &out));
  EXPECT_TRUE(out);

  buf[0] = 0;
  EXPECT_TRUE(ParseBool(value, &out));
  EXPECT_FALSE(out);

  buf[0] = 1;
  EXPECT_FALSE(ParseBool(value, &out));
  EXPECT_TRUE(ParseBoolRelaxed(value, &out));
  EXPECT_TRUE(out);

  buf[0] = 0xFF;
  value = Input(buf, 2);
  EXPECT_FALSE(ParseBool(value, &out));
  value = Input(buf, 0);
  EXPECT_FALSE(ParseBool(value, &out));
}

TEST(ParseValuesTest, ParseTimes) {
  GeneralizedTime out;

  EXPECT_TRUE(ParseUTCTime(Input("140218161200Z"), &out));

  // DER-encoded UTCTime must end with 'Z'.
  EXPECT_FALSE(ParseUTCTime(Input("140218161200X"), &out));

  // Check that a negative number (-4 in this case) doesn't get parsed as
  // a 2-digit number.
  EXPECT_FALSE(ParseUTCTime(Input("-40218161200Z"), &out));

  // Check that numbers with a leading 0 don't get parsed in octal by making
  // the second digit an invalid octal digit (e.g. 09).
  EXPECT_TRUE(ParseUTCTime(Input("090218161200Z"), &out));

  // Check that the length is validated.
  EXPECT_FALSE(ParseUTCTime(Input("140218161200"), &out));
  EXPECT_FALSE(ParseUTCTime(Input("140218161200Z0"), &out));
  EXPECT_FALSE(ParseUTCTimeRelaxed(Input("140218161200"), &out));
  EXPECT_FALSE(ParseUTCTimeRelaxed(Input("140218161200Z0"), &out));

  // Check strictness of UTCTime parsers.
  EXPECT_FALSE(ParseUTCTime(Input("1402181612Z"), &out));
  EXPECT_TRUE(ParseUTCTimeRelaxed(Input("1402181612Z"), &out));

  // Check that the time ends in Z.
  EXPECT_FALSE(ParseUTCTimeRelaxed(Input("1402181612Z0"), &out));

  // Check format of GeneralizedTime.

  // Leap seconds are allowed.
  EXPECT_TRUE(ParseGeneralizedTime(Input("20140218161260Z"), &out));

  // But nothing larger than a leap second.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140218161261Z"), &out));

  // Minutes only go up to 59.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140218166000Z"), &out));

  // Hours only go up to 23.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140218240000Z"), &out));
  // The 0th day of a month is invalid.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140200161200Z"), &out));
  // The 0th month is invalid.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140018161200Z"), &out));
  // Months greater than 12 are invalid.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20141318161200Z"), &out));

  // Some months have 31 days.
  EXPECT_TRUE(ParseGeneralizedTime(Input("20140131000000Z"), &out));

  // September has only 30 days.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140931000000Z"), &out));

  // February has only 28 days...
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140229000000Z"), &out));

  // ... unless it's a leap year.
  EXPECT_TRUE(ParseGeneralizedTime(Input("20160229000000Z"), &out));

  // There aren't any leap days in years divisible by 100...
  EXPECT_FALSE(ParseGeneralizedTime(Input("21000229000000Z"), &out));

  // ...unless it's also divisible by 400.
  EXPECT_TRUE(ParseGeneralizedTime(Input("20000229000000Z"), &out));

  // Check more perverse invalid inputs.

  const uint8_t trailing_null_bytes[] = {'2',
                                         '0',
                                         '0',
                                         '0',
                                         '1',
                                         '2',
                                         '3',
                                         '1',
                                         '0',
                                         '1',
                                         '0',
                                         '2',
                                         '0',
                                         '3',
                                         'Z',
                                         '\0'};
  Input trailing_null(trailing_null_bytes, sizeof(trailing_null_bytes));
  EXPECT_FALSE(ParseGeneralizedTime(trailing_null, &out));
  const uint8_t embedded_null_bytes[] = {'2',
                                         '0',
                                         '0',
                                         '\0',
                                         '1',
                                         '2',
                                         '3',
                                         '1',
                                         '0',
                                         '1',
                                         '0',
                                         '2',
                                         '0',
                                         '3',
                                         'Z'};
  Input embedded_null(embedded_null_bytes, sizeof(embedded_null_bytes));
  EXPECT_FALSE(ParseGeneralizedTime(embedded_null, &out));

  // The year can't be in hex.
  EXPECT_FALSE(ParseGeneralizedTime(Input("0x201231000000Z"), &out));

  // The last byte must be 'Z'.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20001231000000X"), &out));

  // Check that the length is validated.
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140218161200"), &out));
  EXPECT_FALSE(ParseGeneralizedTime(Input("20140218161200Z0"), &out));
}

TEST(ParseValuesTest, TimesCompare) {
  GeneralizedTime time1;
  GeneralizedTime time2;
  GeneralizedTime time3;

  ASSERT_TRUE(ParseGeneralizedTime(Input("20140218161200Z"), &time1));
  ASSERT_TRUE(ParseUTCTime(Input("150218161200Z"), &time2));
  ASSERT_TRUE(ParseGeneralizedTime(Input("20160218161200Z"), &time3));
  EXPECT_TRUE(time1 < time2);
  EXPECT_TRUE(time2 < time3);
  EXPECT_TRUE(time1 < time3);
}

struct Uint64TestData {
  bool should_pass;
  const uint8_t input[9];
  size_t length;
  uint64_t expected_value;
};

const Uint64TestData kUint64TestData[] = {
    {true, {0x00}, 1, 0},
    {true, {0x01}, 1, 1},
    {false, {0xFF}, 1, 0},
    {true, {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 8, INT64_MAX},
    {false, {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, 0},
    {false, {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 9, 0},
    {false, {0x00, 0x01}, 2, 1},
    {false, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}, 9, 0},
    {false, {0}, 0, 0},
};

TEST(ParseValuesTest, ParseUint64) {
  for (size_t i = 0; i < arraysize(kUint64TestData); i++) {
    Uint64TestData test_case = kUint64TestData[i];
    SCOPED_TRACE(i);

    uint64_t result;
    EXPECT_EQ(test_case.should_pass,
              ParseUint64(Input(test_case.input, test_case.length), &result));
    if (test_case.should_pass)
      EXPECT_EQ(test_case.expected_value, result);
  }
}

}  // namespace test
}  // namespace der
}  // namespace net
