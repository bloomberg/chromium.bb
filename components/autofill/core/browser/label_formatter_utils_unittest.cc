// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter_utils.h"

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

TEST(LabelFormatterUtilsTest, ConstructLabelLine) {
  EXPECT_EQ(base::string16(), ConstructLabelLine({}));

  base::string16 name = base::ASCIIToUTF16("Blaise Pascal");
  base::string16 phone = base::ASCIIToUTF16("01 53 01 82 00");
  base::string16 email = base::ASCIIToUTF16("b.pascal@orange.fr");

  base::string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SUGGESTION_LABEL_SEPARATOR);

  EXPECT_EQ(name, ConstructLabelLine({name}));
  EXPECT_EQ(base::JoinString({name, separator, phone, separator, email},
                             base::string16()),
            ConstructLabelLine({name, phone, email}));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddressesWithOneProfileAndNoEmailAddress) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddressesWithOneProfileAndEmailAddress) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch", "", "", "", "",
                       "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddressesWithProfilesAndNoEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddressesWithProfilesAndSameEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddressesWithProfilesAndDifferentNonEmptyEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann",
                       "mmw@gmail.com", "", "", "", "", "", "", "DE", "");
  EXPECT_FALSE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSameEmailAddressesWithProfilesAndNonEmptyAndEmptyEmailAddresses) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch",
                       "mmkirch@gmx.de", "", "", "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_FALSE(HaveSameEmailAddresses({&profile1, &profile2}, "de"));
  EXPECT_FALSE(HaveSameEmailAddresses({&profile2, &profile1}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbersWithOneProfileAndNoPhoneNumber) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch", "", "", "", "",
                       "", "", "", "DE", "");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbersWithOneProfileAndPhoneNumber) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Maria", "Margaretha", "Kirch", "", "", "", "",
                       "", "", "", "DE", "+49 30 4504-2823");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbersWithProfilesAndNoPhoneNumber) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbersWithProfilesAndSamePhoneNumbers) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "+49 30 4504-2823");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "4903045042823");
  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "03045042823");
  EXPECT_TRUE(HaveSamePhoneNumbers({&profile1, &profile2, &profile3}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbersWithProfilesAndDifferentNonEmptyPhoneNumbers) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "+49 30 4504-2823");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "+49 221 22123828");
  EXPECT_FALSE(HaveSamePhoneNumbers({&profile1, &profile2}, "de"));
}

TEST(LabelFormatterUtilsTest,
     HaveSamePhoneNumbersWithProfilesAndNonEmptyAndEmptyPhoneNumbers) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Maria", "Margaretha", "Kirch", "", "", "",
                       "", "", "", "", "DE", "+49 30 4504-2823");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Maria", "Margaretha", "Winckelmann", "", "",
                       "", "", "", "", "", "DE", "");
  EXPECT_FALSE(HaveSamePhoneNumbers({&profile1, &profile2}, "de"));
  EXPECT_FALSE(HaveSamePhoneNumbers({&profile2, &profile1}, "de"));
}

}  // namespace
}  // namespace autofill
