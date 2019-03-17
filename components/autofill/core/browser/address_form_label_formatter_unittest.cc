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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace autofill {
namespace {

std::vector<ServerFieldType> GetFieldTypes() {
  return {NAME_FIRST,
          NAME_LAST,
          ADDRESS_HOME_LINE1,
          ADDRESS_HOME_LINE2,
          ADDRESS_HOME_DEPENDENT_LOCALITY,
          ADDRESS_HOME_CITY,
          ADDRESS_HOME_STATE,
          ADDRESS_HOME_ZIP,
          ADDRESS_HOME_COUNTRY};
}

TEST(AddressFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_FIRST, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels(std::vector<AutofillProfile*>()).empty());
}

TEST(AddressFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedAddress) {
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
      LabelFormatter::Create("en-US", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(base::ASCIIToUTF16("John F Kennedy"),
                  base::ASCIIToUTF16("Jackie Kennedy")));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonAddress) {
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

TEST(AddressFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(base::ASCIIToUTF16("Tarsila do Amaral"),
                  base::ASCIIToUTF16("Artur Avila")));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", NAME_FIRST, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(
          base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301, Vila Mariana, São "
                            "Paulo-SP, 04094-050"),
          base::UTF8ToUTF16(
              "Estr. Dona Castorina, 110, Jardim Botânico, Rio de "
              "Janeiro-RJ, 22460-320")));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForProfilesWithIncompleteNamesAndFocusedAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "", "", "", "", "", "19 N Square", "",
                       "Boston", "MA", "02113", "US", "");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "", "", "Adams", "", "", "141 Franklin St",
                       "", "Quincy", "MA", "02169", "US", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", ADDRESS_HOME_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(base::string16(), base::ASCIIToUTF16("Adams")));
}

TEST(AddressFormLabelFormatterTest,
     GetLabelsForProfilesWithIncompleteAddressAndFocusedNonAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "", "", "", "", "", "", "");
  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "John", "", "Adams", "", "",
                       "141 Franklin St", "", "", "", "", "", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_FIRST, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(base::string16(), base::ASCIIToUTF16("141 Franklin St")));
}

}  // namespace
}  // namespace autofill