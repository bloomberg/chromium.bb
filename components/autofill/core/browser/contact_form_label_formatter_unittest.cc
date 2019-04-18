// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_form_label_formatter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace autofill {
namespace {

std::vector<ServerFieldType> GetNamePhoneAndEmailFieldTypes() {
  return {NAME_FIRST, NAME_LAST, PHONE_HOME_WHOLE_NUMBER, EMAIL_ADDRESS};
}

TEST(ContactFormLabelFormatterTest, GetLabelsWithMissingProfiles) {
  const std::vector<AutofillProfile*> profiles{};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", NAME_FIRST, GetNamePhoneAndEmailFieldTypes(), profiles);
  EXPECT_TRUE(formatter->GetLabels(profiles).empty());
}

TEST(ContactFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedName) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "", "", "19 N Square",
                       "", "Boston", "MA", "02113", "US", "+1 (617) 523-2338");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "John", "", "Adams", "", "",
                       "141 Franklin St.", "", "Quincy", "MA", "02169", "US",
                       "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", NAME_LAST, GetNamePhoneAndEmailFieldTypes(), profiles);

  EXPECT_THAT(
      formatter->GetLabels(profiles),
      ElementsAre(ConstructLabelLine({base::ASCIIToUTF16("(617) 730-2000"),
                                      base::ASCIIToUTF16("jfk@gmail.com")}),
                  base::ASCIIToUTF16("jackie@outlook.com"),
                  base::ASCIIToUTF16("(617) 523-2338"), base::string16()));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedEmail) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "", "", "19 N Square",
                       "", "Boston", "MA", "02113", "US", "+1 (617) 523-2338");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St.", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", EMAIL_ADDRESS, GetNamePhoneAndEmailFieldTypes(), profiles);

  EXPECT_THAT(
      formatter->GetLabels(profiles),
      ElementsAre(ConstructLabelLine({base::ASCIIToUTF16("John F Kennedy"),
                                      base::ASCIIToUTF16("(617) 730-2000")}),
                  base::ASCIIToUTF16("Jackie Kennedy"),
                  ConstructLabelLine({base::ASCIIToUTF16("Paul Revere"),
                                      base::ASCIIToUTF16("(617) 523-2338")}),
                  base::string16()));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForUSProfilesAndFocusedPhone) {
  AutofillProfile profile1 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile1, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  AutofillProfile profile2 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile2, "Jackie", "", "Kennedy", "jackie@outlook.com",
                       "", "151 Irving Ave", "", "Hyannis", "MA", "02601", "US",
                       "");

  AutofillProfile profile3 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile3, "Paul", "", "Revere", "", "", "19 N Square",
                       "", "Boston", "MA", "02113", "US", "+1 (617) 523-2338");

  AutofillProfile profile4 =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile4, "", "", "", "", "", "141 Franklin St.", "",
                       "Quincy", "MA", "02169", "US", "");

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2, &profile3,
                                               &profile4};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("en-US", PHONE_HOME_WHOLE_NUMBER,
                             GetNamePhoneAndEmailFieldTypes(), profiles);

  EXPECT_THAT(
      formatter->GetLabels(profiles),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("John F Kennedy"),
                              base::ASCIIToUTF16("jfk@gmail.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Jackie Kennedy"),
                              base::ASCIIToUTF16("jackie@outlook.com")}),
          base::ASCIIToUTF16("Paul Revere"), base::string16()));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedName) {
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

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "pt-BR", NAME_LAST, GetNamePhoneAndEmailFieldTypes(), profiles);

  EXPECT_THAT(
      formatter->GetLabels(profiles),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("(11) 2648-0254"),
                              base::ASCIIToUTF16("tarsila@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("(21) 98765-0000"),
                              base::ASCIIToUTF16("aavila@uol.com.br")})));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedEmail) {
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

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "pt-BR", EMAIL_ADDRESS, GetNamePhoneAndEmailFieldTypes(), profiles);

  EXPECT_THAT(
      formatter->GetLabels(profiles),
      ElementsAre(ConstructLabelLine({base::ASCIIToUTF16("Tarsila do Amaral"),
                                      base::ASCIIToUTF16("(11) 2648-0254")}),
                  ConstructLabelLine({base::ASCIIToUTF16("Artur Avila"),
                                      base::ASCIIToUTF16("(21) 98765-0000")})));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForBRProfilesAndFocusedPhone) {
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

  const std::vector<AutofillProfile*> profiles{&profile1, &profile2};
  const std::unique_ptr<LabelFormatter> formatter =
      LabelFormatter::Create("pt-BR", PHONE_HOME_WHOLE_NUMBER,
                             GetNamePhoneAndEmailFieldTypes(), profiles);

  EXPECT_THAT(
      formatter->GetLabels(profiles),
      ElementsAre(
          ConstructLabelLine({base::ASCIIToUTF16("Tarsila do Amaral"),
                              base::ASCIIToUTF16("tarsila@aol.com")}),
          ConstructLabelLine({base::ASCIIToUTF16("Artur Avila"),
                              base::ASCIIToUTF16("aavila@uol.com.br")})));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndPhoneWithFocusedName) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", NAME_LAST, {NAME_FIRST, NAME_LAST, PHONE_HOME_WHOLE_NUMBER},
      profiles);

  // Checks that the email address is excluded when the form does not contain an
  // email field.
  EXPECT_THAT(formatter->GetLabels(profiles),
              ElementsAre(base::ASCIIToUTF16("(617) 730-2000")));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndPhoneWithFocusedPhone) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", PHONE_HOME_WHOLE_NUMBER,
      {NAME_FIRST, NAME_LAST, PHONE_HOME_WHOLE_NUMBER}, profiles);

  // Checks that the email address is excluded when the form does not contain an
  // email field.
  EXPECT_THAT(formatter->GetLabels(profiles),
              ElementsAre(base::ASCIIToUTF16("John F Kennedy")));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndEmailWithFocusedName) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", NAME_LAST, {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS}, profiles);

  // Checks that the phone number is excluded when the form does not contain a
  // phone field.
  EXPECT_THAT(formatter->GetLabels(profiles),
              ElementsAre(base::ASCIIToUTF16("jfk@gmail.com")));
}

TEST(ContactFormLabelFormatterTest, GetLabelsForNameAndEmailWithFocusedEmail) {
  AutofillProfile profile =
      AutofillProfile(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&profile, "John", "F", "Kennedy", "jfk@gmail.com", "",
                       "333 Washington St", "", "Brookline", "MA", "02445",
                       "US", "16177302000");

  const std::vector<AutofillProfile*> profiles{&profile};
  const std::unique_ptr<LabelFormatter> formatter = LabelFormatter::Create(
      "en-US", EMAIL_ADDRESS, {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS}, profiles);

  // Checks that the phone number is excluded when the form does not contain a
  // phone field.
  EXPECT_THAT(formatter->GetLabels(profiles),
              ElementsAre(base::ASCIIToUTF16("John F Kennedy")));
}

}  // namespace
}  // namespace autofill