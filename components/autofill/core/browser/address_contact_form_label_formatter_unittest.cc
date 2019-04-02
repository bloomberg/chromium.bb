// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_contact_form_label_formatter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter_test_utils.h"
#include "components/autofill/core/browser/label_formatter_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace autofill {
namespace {

std::vector<ServerFieldType> GetFieldTypes() {
  return {NO_SERVER_DATA,
          NAME_BILLING_FULL,
          EMAIL_ADDRESS,
          ADDRESS_BILLING_LINE1,
          ADDRESS_BILLING_LINE2,
          ADDRESS_BILLING_DEPENDENT_LOCALITY,
          ADDRESS_BILLING_CITY,
          ADDRESS_BILLING_STATE,
          ADDRESS_BILLING_ZIP,
          ADDRESS_BILLING_COUNTRY,
          PHONE_BILLING_WHOLE_NUMBER};
}

TEST(AddressContactFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_BILLING_FULL, GetFieldTypes());
  EXPECT_TRUE(formatter->GetLabels(std::vector<AutofillProfile*>()).empty());
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Dina", "", "Katabi", "dina@mit.edu", "", "",
                       "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", NAME_BILLING_FULL, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{
          &profile1, &profile2, &profile3, &profile4, &profile5, &profile6}),
      ElementsAre(
          ConstructLabelLines(
              base::ASCIIToUTF16("19 North Sq, Boston, MA 02113"),
              FormatExpectedLabel("(617) 523-2338", "sarah.revere@aol.com")),
          ConstructLabelLines(
              base::ASCIIToUTF16("151 Irving Ave, Hyannis, MA 02601"),
              base::ASCIIToUTF16("(617) 514-1600")),
          ConstructLabelLines(
              base::ASCIIToUTF16("19 North Sq, Boston, MA 02113"),
              base::ASCIIToUTF16("paul1775@gmail.com")),
          FormatExpectedLabel("(617) 324-0000", "dina@mit.edu"),
          base::ASCIIToUTF16(
              "Old North Church, 193 Salem St, Boston, MA 02113"),
          base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Dina", "", "Katabi", "dina@mit.edu", "", "",
                       "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", ADDRESS_BILLING_LINE1, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{
          &profile1, &profile2, &profile3, &profile4, &profile5, &profile6}),
      ElementsAre(
          ConstructLabelLines(
              FormatExpectedLabel("Sarah Revere", "Boston, MA 02113"),
              FormatExpectedLabel("(617) 523-2338", "sarah.revere@aol.com")),
          ConstructLabelLines(
              FormatExpectedLabel("Jackie L Kennedy", "Hyannis, MA 02601"),
              base::ASCIIToUTF16("(617) 514-1600")),
          ConstructLabelLines(
              FormatExpectedLabel("Paul Revere", "Boston, MA 02113"),
              base::ASCIIToUTF16("paul1775@gmail.com")),
          ConstructLabelLines(
              base::ASCIIToUTF16("Dina Katabi"),
              FormatExpectedLabel("(617) 324-0000", "dina@mit.edu")),
          base::ASCIIToUTF16("Boston, MA 02113"), base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Dina", "", "Katabi", "dina@mit.edu", "", "",
                       "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", ADDRESS_BILLING_CITY, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{
          &profile1, &profile2, &profile3, &profile4, &profile5, &profile6}),
      ElementsAre(
          ConstructLabelLines(
              FormatExpectedLabel("Sarah Revere", "19 North Sq"),
              FormatExpectedLabel("(617) 523-2338", "sarah.revere@aol.com")),
          ConstructLabelLines(
              FormatExpectedLabel("Jackie L Kennedy", "151 Irving Ave"),
              base::ASCIIToUTF16("(617) 514-1600")),
          ConstructLabelLines(FormatExpectedLabel("Paul Revere", "19 North Sq"),
                              base::ASCIIToUTF16("paul1775@gmail.com")),
          ConstructLabelLines(
              base::ASCIIToUTF16("Dina Katabi"),
              FormatExpectedLabel("(617) 324-0000", "dina@mit.edu")),
          base::ASCIIToUTF16("Old North Church, 193 Salem St"),
          base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Dina", "", "Katabi", "dina@mit.edu", "", "",
                       "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", EMAIL_ADDRESS, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{
          &profile1, &profile2, &profile3, &profile4, &profile5, &profile6}),
      ElementsAre(ConstructLabelLines(
                      FormatExpectedLabel("Sarah Revere", "(617) 523-2338"),
                      base::ASCIIToUTF16("19 North Sq, Boston, MA 02113")),
                  ConstructLabelLines(
                      FormatExpectedLabel("Jackie L Kennedy", "(617) 514-1600"),
                      base::ASCIIToUTF16("151 Irving Ave, Hyannis, MA 02601")),
                  ConstructLabelLines(
                      base::ASCIIToUTF16("Paul Revere"),
                      base::ASCIIToUTF16("19 North Sq, Boston, MA 02113")),
                  FormatExpectedLabel("Dina Katabi", "(617) 324-0000"),
                  base::ASCIIToUTF16(
                      "Old North Church, 193 Salem St, Boston, MA 02113"),
                  base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForUSProfilesAndFocusedPhone) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "L", "Kennedy", "", "",
                       "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "6175141600");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "paul1775@gmail.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "Dina", "", "Katabi", "dina@mit.edu", "", "",
                       "", "", "", "", "US", "6173240000");

  AutofillProfile profile5 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile5, "", "", "", "", "", "Old North Church",
                       "193 Salem St", "Boston", "MA", "02113", "US", "");

  AutofillProfile profile6 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile6, "", "", "", "", "", "", "", "", "", "", "US",
                       "");

  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", PHONE_BILLING_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{
          &profile1, &profile2, &profile3, &profile4, &profile5, &profile6}),
      ElementsAre(
          ConstructLabelLines(
              FormatExpectedLabel("Sarah Revere", "sarah.revere@aol.com"),
              base::ASCIIToUTF16("19 North Sq, Boston, MA 02113")),
          ConstructLabelLines(
              base::ASCIIToUTF16("Jackie L Kennedy"),
              base::ASCIIToUTF16("151 Irving Ave, Hyannis, MA 02601")),
          ConstructLabelLines(
              FormatExpectedLabel("Paul Revere", "paul1775@gmail.com"),
              base::ASCIIToUTF16("19 North Sq, Boston, MA 02113")),
          FormatExpectedLabel("Dina Katabi", "dina@mit.edu"),
          base::ASCIIToUTF16(
              "Old North Church, 193 Salem St, Boston, MA 02113"),
          base::string16()));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
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
      ElementsAre(
          ConstructLabelLines(
              base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301, Vila Mariana, "
                                "São Paulo-SP, 04094-050"),
              FormatExpectedLabel("(11) 2648-0254", "tarsila@aol.com")),
          ConstructLabelLines(
              base::UTF8ToUTF16("Estr. Dona Castorina, 110, Jardim Botânico, "
                                "Rio de Janeiro-RJ, 22460-320"),
              FormatExpectedLabel("(21) 98765-0000", "aavila@uol.com.br"))));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
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
      ElementsAre(
          ConstructLabelLines(
              FormatExpectedLabel("Tarsila do Amaral",
                                  "Vila Mariana, São Paulo-SP, 04094-050"),
              FormatExpectedLabel("(11) 2648-0254", "tarsila@aol.com")),
          ConstructLabelLines(
              FormatExpectedLabel(
                  "Artur Avila",
                  "Jardim Botânico, Rio de Janeiro-RJ, 22460-320"),
              FormatExpectedLabel("(21) 98765-0000", "aavila@uol.com.br"))));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedNonStreetAddress) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", ADDRESS_BILLING_ZIP, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(
          ConstructLabelLines(
              FormatExpectedLabel("Tarsila do Amaral",
                                  "Av. Pedro Álvares Cabral, 1301"),
              FormatExpectedLabel("(11) 2648-0254", "tarsila@aol.com")),
          ConstructLabelLines(
              FormatExpectedLabel("Artur Avila", "Estr. Dona Castorina, 110"),
              FormatExpectedLabel("(21) 98765-0000", "aavila@uol.com.br"))));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
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
          ConstructLabelLines(
              FormatExpectedLabel("Tarsila do Amaral", "(11) 2648-0254"),
              base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301, Vila Mariana, "
                                "São Paulo-SP, 04094-050")),
          ConstructLabelLines(
              FormatExpectedLabel("Artur Avila", "(21) 98765-0000"),
              base::UTF8ToUTF16("Estr. Dona Castorina, 110, Jardim Botânico, "
                                "Rio de Janeiro-RJ, 22460-320"))));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForBRProfilesAndFocusedPhone) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "Tarsila", "do", "Amaral", "tarsila@aol.com",
                       "", "Av. Pedro Álvares Cabral, 1301", "", "Vila Mariana",
                       "São Paulo", " SP ", " 04094-050 ", "BR",
                       "+55 11 2648-0254");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Artur", "", "Avila", "aavila@uol.com.br", "",
                       "Estr. Dona Castorina, 110", "", "Jardim Botânico",
                       "Rio de Janeiro", "RJ", "22460-320", "BR",
                       "21987650000");

  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "pt-BR", PHONE_BILLING_WHOLE_NUMBER, GetFieldTypes());

  EXPECT_THAT(
      formatter->GetLabels(std::vector<AutofillProfile*>{&profile1, &profile2}),
      ElementsAre(
          ConstructLabelLines(
              FormatExpectedLabel("Tarsila do Amaral", "tarsila@aol.com"),
              base::UTF8ToUTF16("Av. Pedro Álvares Cabral, 1301, Vila Mariana, "
                                "São Paulo-SP, 04094-050")),
          ConstructLabelLines(
              FormatExpectedLabel("Artur Avila", "aavila@uol.com.br"),
              base::UTF8ToUTF16("Estr. Dona Castorina, 110, Jardim Botânico, "
                                "Rio de Janeiro-RJ, 22460-320"))));
}

TEST(AddressContactFormLabelFormatterTest,
     GetLabelsForFormWithPartialAddressFields) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "Sarah", "", "Revere", "sarah.revere@aol.com",
                       "", "19 North Sq", "", "Boston", "MA", "02113", "US",
                       "16175232338");

  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", EMAIL_ADDRESS,
                             {NAME_BILLING_FULL, EMAIL_ADDRESS,
                              ADDRESS_BILLING_ZIP, PHONE_BILLING_WHOLE_NUMBER});

  // Checks that only address fields in the form are shown in the label.
  EXPECT_THAT(formatter->GetLabels(std::vector<AutofillProfile*>{&profile}),
              ElementsAre(ConstructLabelLines(
                  FormatExpectedLabel("Sarah Revere", "(617) 523-2338"),
                  base::ASCIIToUTF16("02113"))));
}

}  // namespace
}  // namespace autofill
