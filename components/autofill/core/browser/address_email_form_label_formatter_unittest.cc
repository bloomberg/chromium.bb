// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_email_form_label_formatter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter_utils.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::ElementsAre;

namespace autofill {
namespace {

std::vector<ServerFieldType> GetFieldTypes() {
  return {NAME_FULL,
          EMAIL_ADDRESS,
          ADDRESS_BILLING_LINE1,
          ADDRESS_BILLING_LINE2,
          ADDRESS_BILLING_CITY,
          ADDRESS_BILLING_STATE,
          ADDRESS_BILLING_ZIP,
          ADDRESS_BILLING_COUNTRY};
}

base::string16 FormatExpectedLabel(base::StringPiece label_part1,
                                   base::StringPiece label_part2) {
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_SUGGESTION_LABEL,
                                    base::UTF8ToUTF16(label_part1),
                                    base::UTF8ToUTF16(label_part2));
}

TEST(AddressEmailFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_BILLING_FULL, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels(std::vector<AutofillProfile*>()).empty());
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonAddressNonEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "", "", "", "", "", "US", "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "John", "", "Adams", "", "", "", "", "Quincy",
                       "MA", "02169", "US", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_BILLING_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2,
                                                         &profile3, &profile4}),
      ElementsAre(FormatExpectedLabel("333 Washington St", "jfk@gmail.com"),
                  base::ASCIIToUTF16("151 Irving Ave"),
                  base::ASCIIToUTF16("paul1775@gmail.com"), base::string16()));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "", "", "", "paul1775@gmail.com", "", "", "",
                       "", "", "", "US", "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", ADDRESS_BILLING_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2,
                                                         &profile3, &profile4}),
      ElementsAre(FormatExpectedLabel("John F Kennedy", "jfk@gmail.com"),
                  base::ASCIIToUTF16("Jackie Kennedy"),
                  base::ASCIIToUTF16("paul1775@gmail.com"), base::string16()));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "", "", "Hyannis", "MA", "02601", "US", "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "", "", "", "paul1775@gmail.com", "", "", "",
                       "", "", "", "US", "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2,
                                                         &profile3, &profile4}),
      ElementsAre(FormatExpectedLabel("John F Kennedy", "333 Washington St"),
                  base::ASCIIToUTF16("Jackie Kennedy"), base::string16(),
                  base::ASCIIToUTF16("141 Franklin St")));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonAddressNonEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", NAME_BILLING_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(FormatExpectedLabel("Av. Pedro Álvares Cabral, 1301",
                                      "tarsila@aol.com"),
                  FormatExpectedLabel("Estr. Dona Castorina, 110",
                                      "aavila@uol.com.br")));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", ADDRESS_BILLING_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(FormatExpectedLabel("Tarsila do Amaral", "tarsila@aol.com"),
                  FormatExpectedLabel("Artur Avila", "aavila@uol.com.br")));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", "SP", "04094-050", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(
          FormatExpectedLabel("Tarsila do Amaral",
                              "Av. Pedro Álvares Cabral, 1301"),
          FormatExpectedLabel("Artur Avila", "Estr. Dona Castorina, 110")));
}

TEST(AddressEmailFormLabelFormatterTest,
     GetLabelsForFormWithAddressFieldsMinusStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", EMAIL_ADDRESS,
                             {NAME_BILLING_FULL, EMAIL_ADDRESS,
                              ADDRESS_BILLING_CITY, ADDRESS_BILLING_STATE});

  // Checks that only address fields in the form are shown in the label.
  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1}),
      ElementsAre(FormatExpectedLabel("John F Kennedy", "Brookline, MA")));
}

}  // namespace
}  // namespace autofill
