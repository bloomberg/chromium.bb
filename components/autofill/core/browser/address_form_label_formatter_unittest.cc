// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_form_label_formatter.h"

#include <vector>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace autofill {
namespace {

std::vector<ServerFieldType> GetFieldTypes() {
  return {NO_SERVER_DATA,     NAME_FIRST,
          NAME_LAST,          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2, ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,  ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,   ADDRESS_HOME_COUNTRY};
}

TEST(AddressFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_FIRST, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels(std::vector<AutofillProfile*>()).empty());
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "", "", "jackie@outlook.com", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "", "US",
                       "5087717796");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "", "", "", "", "", "US", "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2,
                                                         &profile3, &profile4}),
      ElementsAre(FormatExpectedLabel("John F Kennedy", "Brookline, MA 02445"),
                  base::ASCIIToUTF16("Hyannis, MA"),
                  base::ASCIIToUTF16("Paul Revere"), base::string16()));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "", "", "jackie@outlook.com", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "", "US",
                       "5087717796");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "", "", "", "", "", "US", "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", ADDRESS_HOME_CITY, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2,
                                                         &profile3, &profile4}),
      ElementsAre(FormatExpectedLabel("John F Kennedy", "333 Washington St"),
                  base::ASCIIToUTF16("151 Irving Ave"),
                  base::ASCIIToUTF16("Paul Revere"), base::string16()));
}

TEST(AddressFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "5087717796");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_FIRST, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(base::ASCIIToUTF16("333 Washington St, Brookline, MA 02445"),
                  base::ASCIIToUTF16("151 Irving Ave, Hyannis, MA 02601")));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(std::vector<AutofillProfile*>{&profile1}),
              ElementsAre(FormatExpectedLabel("Tarsila do Amaral",
                                              "Vila Mariana, São "
                                              "Paulo-SP, 04094-050")));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", ADDRESS_HOME_ZIP, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(std::vector<AutofillProfile*>{&profile1}),
              ElementsAre(FormatExpectedLabel(
                  "Tarsila do Amaral", "Av. Pedro Álvares Cabral, 1301")));
}

TEST(AddressFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", NAME_FIRST, GetFieldTypes());

  EXPECT_THAT(formatter->GetLabels(std::vector<AutofillProfile*>{&profile1}),
              ElementsAre(base::UTF8ToUTF16(
                  "Av. Pedro Álvares Cabral, 1301, Vila Mariana, São "
                  "Paulo-SP, 04094-050")));
}

}  // namespace
}  // namespace autofill
