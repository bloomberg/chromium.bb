// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/phone_validation_util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillPhoneValidationTest : public testing::Test {
 public:
  AutofillPhoneValidationTest() {}

  AutofillProfile::ValidityState ValidatePhoneTest(AutofillProfile* profile) {
    return phone_validation_util::ValidatePhoneNumber(profile);
  }

  ~AutofillPhoneValidationTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPhoneValidationTest);
};

TEST_F(AutofillPhoneValidationTest, ValidateNULLProfile) {
  EXPECT_EQ(AutofillProfile::UNVALIDATED, ValidatePhoneTest(nullptr));
}

TEST_F(AutofillPhoneValidationTest, ValidateFullValidProfile) {
  // This is a full valid profile:
  // Country Code: "CA", Phone Number: "15141112233"
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(AutofillPhoneValidationTest, ValidateEmptyPhoneNumber) {
  // This is a profile with empty phone number. Since phone number field is
  // always required, it is considered as invalid.
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::string16());
  EXPECT_EQ(AutofillProfile::INVALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(AutofillPhoneValidationTest, ValidateValidPhone_CountryCodeNotExist) {
  // This is a profile with invalid country code, therefore the phone number
  // cannot be validated.
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  EXPECT_EQ(AutofillProfile::UNVALIDATED, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(AutofillPhoneValidationTest, ValidateEmptyPhone_CountryCodeNotExist) {
  // This is a profile with invalid country code, but a missing phone number.
  // Therefore, it's an invalid phone number.
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::string16());
  EXPECT_EQ(AutofillProfile::INVALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(AutofillPhoneValidationTest, ValidateInvalidPhoneNumber) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16("33"));
  EXPECT_EQ(AutofillProfile::INVALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("151411122334"));
  EXPECT_EQ(AutofillProfile::INVALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("1(514)111-22-334"));
  EXPECT_EQ(AutofillProfile::INVALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("251411122334"));
  EXPECT_EQ(AutofillProfile::INVALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16("Hello!"));
  EXPECT_EQ(AutofillProfile::INVALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(AutofillPhoneValidationTest, ValidateValidPhoneNumber) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16("5141112233"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("514-111-2233"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("1(514)111-22-33"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("+1 514 111 22 33"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("+1 (514)-111-22-33"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("(514)-111-22-33"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("+1 650 GOO OGLE"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                     base::ASCIIToUTF16("778 111 22 33"));
  EXPECT_EQ(AutofillProfile::VALID, ValidatePhoneTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(PHONE_HOME_WHOLE_NUMBER));
}

}  // namespace autofill
