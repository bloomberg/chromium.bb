// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace autofill_i18n;

TEST(PhoneNumberI18NTest, PhoneNumbersMatch) {
  // Same numbers, defined country code.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("4158889999"),
                                "US"));
  // Same numbers, undefined country code.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("4158889999"),
                                ""));

  // Numbers differ by country code only.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                ASCIIToUTF16("4158889999"),
                                "US"));

  // Same numbers, different formats.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415-888-9999"),
                                "US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("(415)888-9999"),
                                "US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415 888 9999"),
                                "US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415 TUV WXYZ"),
                                "US"));

  // Partial matches don't count.
  EXPECT_FALSE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                 ASCIIToUTF16("8889999"),
                                 "US"));
}
