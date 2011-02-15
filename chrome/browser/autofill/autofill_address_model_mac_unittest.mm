// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef CocoaTest AutoFillAddressModelTest;

TEST(AutoFillAddressModelTest, Basic) {
  // A basic test that creates a new instance and releases.
  // Aids valgrind leak detection.
  AutoFillProfile profile;
  scoped_nsobject<AutoFillAddressModel> model([[AutoFillAddressModel alloc]
      initWithProfile:profile]);
  EXPECT_TRUE(model.get());
}

TEST(AutoFillAddressModelTest, InitializationFromProfile) {
  AutoFillProfile profile;
  autofill_test::SetProfileInfo(
      &profile,
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
  scoped_nsobject<AutoFillAddressModel> model([[AutoFillAddressModel alloc]
      initWithProfile:profile]);
  EXPECT_TRUE(model.get());

  EXPECT_TRUE([[model fullName] isEqualToString:@"Marion Mitchell Morrison"]);
  EXPECT_TRUE([[model email] isEqualToString:@"johnwayne@me.xyz"]);
  EXPECT_TRUE([[model companyName] isEqualToString:@"Fox"]);
  EXPECT_TRUE([[model addressLine1] isEqualToString:@"123 Zoo St."]);
  EXPECT_TRUE([[model addressLine2] isEqualToString:@"unit 5"]);
  EXPECT_TRUE([[model addressCity] isEqualToString:@"Hollywood"]);
  EXPECT_TRUE([[model addressState] isEqualToString:@"CA"]);
  EXPECT_TRUE([[model addressZip] isEqualToString:@"91601"]);
  EXPECT_TRUE([[model addressCountry] isEqualToString:@"US"]);
  EXPECT_TRUE([[model phoneWholeNumber] isEqualToString:@"12345678910"]);
  EXPECT_TRUE([[model faxWholeNumber] isEqualToString:@"01987654321"]);
}

TEST(AutoFillAddressModelTest, CopyModelToProfile) {
  AutoFillProfile profile;
  autofill_test::SetProfileInfo(
      &profile,
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
  scoped_nsobject<AutoFillAddressModel> model([[AutoFillAddressModel alloc]
      initWithProfile:profile]);
  EXPECT_TRUE(model.get());

  [model setFullName:@"MarionX MitchellX MorrisonX"];
  [model setEmail:@"trigger@me.xyz"];
  [model setCompanyName:@"FoxX"];
  [model setAddressLine1:@"123 Xoo St."];
  [model setAddressLine2:@"unit 5X"];
  [model setAddressCity:@"Seattle"];
  [model setAddressState:@"WA"];
  [model setAddressZip:@"81601"];
  [model setAddressCountry:@"CA"];
  [model setPhoneWholeNumber:@"23346678910"];
  [model setFaxWholeNumber:@"12988654321"];

  [model copyModelToProfile:&profile];

  EXPECT_EQ(ASCIIToUTF16("MarionX"),
            profile.GetFieldText(AutoFillType(NAME_FIRST)));
  EXPECT_EQ(ASCIIToUTF16("MitchellX"),
            profile.GetFieldText(AutoFillType(NAME_MIDDLE)));
  EXPECT_EQ(ASCIIToUTF16("MorrisonX"),
            profile.GetFieldText(AutoFillType(NAME_LAST)));
  EXPECT_EQ(ASCIIToUTF16("MarionX MitchellX MorrisonX"),
            profile.GetFieldText(AutoFillType(NAME_FULL)));
  EXPECT_EQ(ASCIIToUTF16("trigger@me.xyz"),
            profile.GetFieldText(AutoFillType(EMAIL_ADDRESS)));
  EXPECT_EQ(ASCIIToUTF16("FoxX"),
            profile.GetFieldText(AutoFillType(COMPANY_NAME)));
  EXPECT_EQ(ASCIIToUTF16("123 Xoo St."),
            profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)));
  EXPECT_EQ(ASCIIToUTF16("unit 5X"),
            profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)));
  EXPECT_EQ(ASCIIToUTF16("Seattle"),
            profile.GetFieldText(AutoFillType(ADDRESS_HOME_CITY)));
  EXPECT_EQ(ASCIIToUTF16("WA"),
            profile.GetFieldText(AutoFillType(ADDRESS_HOME_STATE)));
  EXPECT_EQ(ASCIIToUTF16("81601"),
            profile.GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)));
  EXPECT_EQ(ASCIIToUTF16("CA"),
            profile.GetFieldText(AutoFillType(ADDRESS_HOME_COUNTRY)));
  EXPECT_EQ(ASCIIToUTF16("23346678910"),
            profile.GetFieldText(AutoFillType(PHONE_HOME_WHOLE_NUMBER)));
  EXPECT_EQ(ASCIIToUTF16("12988654321"),
            profile.GetFieldText(AutoFillType(PHONE_FAX_WHOLE_NUMBER)));
}

}  // namespace
