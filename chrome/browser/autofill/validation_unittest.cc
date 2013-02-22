// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/validation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// From https://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
const char* const kValidNumbers[] = {
  "378282246310005",
  "3714 4963 5398 431",
  "3787-3449-3671-000",
  "5610591081018250",
  "3056 9309 0259 04",
  "3852-0000-0232-37",
  "6011111111111117",
  "6011 0009 9013 9424",
  "3530-1113-3330-0000",
  "3566002020360505",
  "5555 5555 5555 4444",
  "5105-1051-0510-5100",
  "4111111111111111",
  "4012 8888 8888 1881",
  "4222-2222-2222-2",
  "5019717010103742",
  "6331101999990016",
};
const char* const kInvalidNumbers[] = {
  "4111 1111 112", /* too short */
  "41111111111111111115", /* too long */
  "4111-1111-1111-1110", /* wrong Luhn checksum */
  "3056 9309 0259 04aa", /* non-digit characters */
};
const char* const kValidCreditCardSecurityCode[] = {
  "323",  // 3-digit CSC.
  "3234",  // 4-digit CSC.
};
const char* const kInvalidCreditCardSecurityCode[] = {
  "32",  // CSC too short.
  "12345",  // CSC too long.
  "asd",  // non-numeric CSC.
};
const char* const kValidEmailAddress[] = {
  "user@example",
  "user@example.com",
  "user@subdomain.example.com",
  "user+postfix@example.com",
};
const char* const kInvalidEmailAddress[] = {
  "user",
  "foo.com",
  "user@",
  "user@=example.com"
};
}  // namespace

TEST(AutofillValidation, IsValidCreditCardNumber) {
  for (size_t i = 0; i < arraysize(kValidNumbers); ++i) {
    SCOPED_TRACE(kValidNumbers[i]);
    EXPECT_TRUE(
        autofill::IsValidCreditCardNumber(ASCIIToUTF16(kValidNumbers[i])));
  }
  for (size_t i = 0; i < arraysize(kInvalidNumbers); ++i) {
    SCOPED_TRACE(kInvalidNumbers[i]);
    EXPECT_FALSE(
        autofill::IsValidCreditCardNumber(ASCIIToUTF16(kInvalidNumbers[i])));
  }
}

TEST(AutofillValidation, IsValidCreditCardSecurityCode) {
  for (size_t i = 0; i < arraysize(kValidCreditCardSecurityCode); ++i) {
    SCOPED_TRACE(kValidCreditCardSecurityCode[i]);
    EXPECT_TRUE(
        autofill::IsValidCreditCardSecurityCode(
            ASCIIToUTF16(kValidCreditCardSecurityCode[i])));
  }
  for (size_t i = 0; i < arraysize(kInvalidCreditCardSecurityCode); ++i) {
    SCOPED_TRACE(kInvalidCreditCardSecurityCode[i]);
    EXPECT_FALSE(
        autofill::IsValidCreditCardSecurityCode(
            ASCIIToUTF16(kInvalidCreditCardSecurityCode[i])));
  }
}

TEST(AutofillValidation, IsValidEmailAddress) {
  for (size_t i = 0; i < arraysize(kValidEmailAddress); ++i) {
    SCOPED_TRACE(kValidEmailAddress[i]);
    EXPECT_TRUE(
        autofill::IsValidEmailAddress(ASCIIToUTF16(kValidEmailAddress[i])));
  }
  for (size_t i = 0; i < arraysize(kInvalidEmailAddress); ++i) {
    SCOPED_TRACE(kInvalidEmailAddress[i]);
    EXPECT_FALSE(
        autofill::IsValidEmailAddress(ASCIIToUTF16(kInvalidEmailAddress[i])));
  }
}


