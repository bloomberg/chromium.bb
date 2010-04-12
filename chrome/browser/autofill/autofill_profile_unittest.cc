// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests different possibilities for summary string generation.
// Based on existence of first name, last name, and address line 1.
TEST(AutoFillProfileTest, PreviewSummaryString) {
  // Case 0/null: ""
  AutoFillProfile profile0(string16(), 0);
  string16 summary0 = profile0.PreviewSummary();
  EXPECT_EQ(string16(), summary0);

  // Case 0/empty: ""
  AutoFillProfile profile00(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile00,
      "Billing",
      "",
      "Mitchell",
      "",
      "johnwayne@me.xyz",
      "Fox",
      "",
      "unit 5",
      "Hollywood", "CA",
      "91601",
      "US",
      "12345678910",
      "01987654321");
  string16 summary00 = profile00.PreviewSummary();
  EXPECT_EQ(string16(), summary00);

  // Case 1: "<address>"
  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile1,
      "Billing",
      "",
      "Mitchell",
      "",
      "johnwayne@me.xyz",
      "Fox",
      "123 Zoo St.",
      "unit 5",
      "Hollywood", "CA",
      "91601",
      "US",
      "12345678910",
      "01987654321");
  string16 summary1 = profile1.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("123 Zoo St.")), summary1);

  // Case 2: "<lastname>"
  AutoFillProfile profile2(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile2,
     "Billing",
     "",
     "Mitchell",
     "Morrison",
     "johnwayne@me.xyz",
     "Fox",
     "",
     "unit 5",
     "Hollywood", "CA",
     "91601",
     "US",
     "12345678910",
     "01987654321");
  string16 summary2 = profile2.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Morrison")), summary2);

  // Case 3: "<lastname>, <address>"
  AutoFillProfile profile3(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile3,
     "Billing",
     "",
     "Mitchell",
     "Morrison",
     "johnwayne@me.xyz",
     "Fox",
     "123 Zoo St.",
     "unit 5",
     "Hollywood", "CA",
     "91601",
     "US",
     "12345678910",
     "01987654321");
  string16 summary3 = profile3.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Morrison, 123 Zoo St.")), summary3);

  // Case 4: "<firstname>"
  AutoFillProfile profile4(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile4,
      "Billing",
      "Marion",
      "Mitchell",
      "",
      "johnwayne@me.xyz",
      "Fox",
      "",
      "unit 5",
      "Hollywood", "CA",
      "91601",
      "US",
      "12345678910",
      "01987654321");
  string16 summary4 = profile4.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion")), summary4);

  // Case 5: "<firstname>, <address>"
  AutoFillProfile profile5(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile5,
      "Billing",
      "Marion",
      "Mitchell",
      "",
      "johnwayne@me.xyz",
      "Fox",
      "123 Zoo St.",
      "unit 5",
      "Hollywood", "CA",
      "91601",
      "US",
      "12345678910",
      "01987654321");
  string16 summary5 = profile5.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion, 123 Zoo St.")), summary5);

  // Case 6: "<firstname> <lastname>"
  AutoFillProfile profile6(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile6,
      "Billing",
      "Marion",
      "Mitchell",
      "Morrison",
      "johnwayne@me.xyz",
      "Fox",
      "",
      "unit 5",
      "Hollywood", "CA",
      "91601",
      "US",
      "12345678910",
      "01987654321");
  string16 summary6 = profile6.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion Morrison")), summary6);

  // Case 7: "<firstname> <lastname>, <address>"
  AutoFillProfile profile7(string16(), 0);
  autofill_unittest::SetProfileInfo(
      &profile7,
      "Billing",
      "Marion",
      "Mitchell",
      "Morrison",
      "johnwayne@me.xyz",
      "Fox",
      "123 Zoo St.",
      "unit 5",
      "Hollywood", "CA",
      "91601",
      "US",
      "12345678910",
      "01987654321");
  string16 summary7 = profile7.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion Morrison, 123 Zoo St.")), summary7);
}

}  // namespace

