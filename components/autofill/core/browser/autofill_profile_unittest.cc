// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/form_field_data.h"
#include "grit/component_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

namespace {

base::string16 GetLabel(AutofillProfile* profile) {
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(profile);
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, &labels);
  return labels[0];
}

}  // namespace

// Tests different possibilities for summary string generation.
// Based on existence of first name, last name, and address line 1.
TEST(AutofillProfileTest, PreviewSummaryString) {
  // Case 0/null: ""
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com/");
  // Empty profile - nothing to update.
  base::string16 summary0 = GetLabel(&profile0);
  EXPECT_EQ(base::string16(), summary0);

  // Case 0a/empty name and address, so the first two fields of the rest of the
  // data is used: "Hollywood, CA"
  AutofillProfile profile00(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile00, "", "", "",
      "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA", "91601", "US",
      "16505678910");
  base::string16 summary00 = GetLabel(&profile00);
  EXPECT_EQ(ASCIIToUTF16("Hollywood, CA"), summary00);

  // Case 1: "<address>" without line 2.
  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile1, "", "", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary1 = GetLabel(&profile1);
  EXPECT_EQ(ASCIIToUTF16("123 Zoo St., Hollywood"), summary1);

  // Case 1a: "<address>" with line 2.
  AutofillProfile profile1a(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile1a, "", "", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary1a = GetLabel(&profile1a);
  EXPECT_EQ(ASCIIToUTF16("123 Zoo St., unit 5"), summary1a);

  // Case 2: "<lastname>"
  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile2, "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary2 = GetLabel(&profile2);
  // Summary includes full name, to the maximal extent available.
  EXPECT_EQ(ASCIIToUTF16("Mitchell Morrison, Hollywood"), summary2);

  // Case 3: "<lastname>, <address>"
  AutofillProfile profile3(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile3, "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "",
      "Hollywood", "CA", "91601", "US", "16505678910");
  base::string16 summary3 = GetLabel(&profile3);
  EXPECT_EQ(ASCIIToUTF16("Mitchell Morrison, 123 Zoo St."), summary3);

  // Case 4: "<firstname>"
  AutofillProfile profile4(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile4, "Marion", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA", "91601", "US",
      "16505678910");
  base::string16 summary4 = GetLabel(&profile4);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell, Hollywood"), summary4);

  // Case 5: "<firstname>, <address>"
  AutofillProfile profile5(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile5, "Marion", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary5 = GetLabel(&profile5);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell, 123 Zoo St."), summary5);

  // Case 6: "<firstname> <lastname>"
  AutofillProfile profile6(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile6, "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "", "Hollywood", "CA",
      "91601", "US", "16505678910");
  base::string16 summary6 = GetLabel(&profile6);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison, Hollywood"),
            summary6);

  // Case 7: "<firstname> <lastname>, <address>"
  AutofillProfile profile7(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile7, "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "16505678910");
  base::string16 summary7 = GetLabel(&profile7);
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison, 123 Zoo St."), summary7);

  // Case 7a: "<firstname> <lastname>, <address>" - same as #7, except for
  // e-mail.
  AutofillProfile profile7a(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile7a, "Marion", "Mitchell",
    "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
    "Hollywood", "CA", "91601", "US", "16505678910");
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile7);
  profiles.push_back(&profile7a);
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, &labels);
  ASSERT_EQ(profiles.size(), labels.size());
  summary7 = labels[0];
  base::string16 summary7a = labels[1];
  EXPECT_EQ(ASCIIToUTF16(
      "Marion Mitchell Morrison, 123 Zoo St., johnwayne@me.xyz"), summary7);
  EXPECT_EQ(ASCIIToUTF16(
      "Marion Mitchell Morrison, 123 Zoo St., marion@me.xyz"), summary7a);
}

TEST(AutofillProfileTest, AdjustInferredLabels) {
  ScopedVector<AutofillProfile> profiles;
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(
      profiles[0],
      "John",
      "",
      "Doe",
      "johndoe@hades.com",
      "Underworld",
      "666 Erebus St.",
      "",
      "Elysium", "CA",
      "91111",
      "US",
      "16502111111");
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "http://www.example.com/"));
  test::SetProfileInfo(
      profiles[1],
      "Jane",
      "",
      "Doe",
      "janedoe@tertium.com",
      "Pluto Inc.",
      "123 Letha Shore.",
      "",
      "Dis", "CA",
      "91222",
      "US",
      "12345678910");
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles.get(), &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);

  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "Chrome settings"));
  test::SetProfileInfo(
      profiles[2],
      "John",
      "",
      "Doe",
      "johndoe@tertium.com",
      "Underworld",
      "666 Erebus St.",
      "",
      "Elysium", "CA",
      "91111",
      "US",
      "16502111111");
  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(profiles.get(), &labels);

  // Profile 0 and 2 inferred label now includes an e-mail.
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@hades.com"),
            labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@tertium.com"),
            labels[2]);

  profiles.resize(2);

  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), std::string()));
  test::SetProfileInfo(
      profiles[2],
      "John",
      "",
      "Doe",
      "johndoe@hades.com",
      "Underworld",
      "666 Erebus St.",
      "",
      "Elysium", "CO",  // State is different
      "91111",
      "US",
      "16502111111");

  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(profiles.get(), &labels);

  // Profile 0 and 2 inferred label now includes a state.
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO"), labels[2]);

  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(
      profiles[3],
      "John",
      "",
      "Doe",
      "johndoe@hades.com",
      "Underworld",
      "666 Erebus St.",
      "",
      "Elysium", "CO",  // State is different for some.
      "91111",
      "US",
      "16504444444");  // Phone is different for some.

  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(profiles.get(), &labels);
  ASSERT_EQ(4U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, 16502111111"),
            labels[2]);
  // This one differs from other ones by unique phone, so no need for extra
  // information.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, 16504444444"),
            labels[3]);

  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(
      profiles[4],
      "John",
      "",
      "Doe",
      "johndoe@styx.com",  // E-Mail is different for some.
      "Underworld",
      "666 Erebus St.",
      "",
      "Elysium", "CO",  // State is different for some.
      "91111",
      "US",
      "16504444444");  // Phone is different for some.

  labels.clear();
  AutofillProfile::CreateDifferentiatingLabels(profiles.get(), &labels);
  ASSERT_EQ(5U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, johndoe@hades.com,"
                         " 16502111111"), labels[2]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, johndoe@hades.com,"
                         " 16504444444"), labels[3]);
  // This one differs from other ones by unique e-mail, so no need for extra
  // information.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., CO, johndoe@styx.com"),
            labels[4]);
}

TEST(AutofillProfileTest, CreateInferredLabels) {
  ScopedVector<AutofillProfile> profiles;
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[0],
                       "John",
                       "",
                       "Doe",
                       "johndoe@hades.com",
                       "Underworld",
                       "666 Erebus St.",
                       "",
                       "Elysium", "CA",
                       "91111",
                       "US",
                       "16502111111");
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[1],
                       "Jane",
                       "",
                       "Doe",
                       "janedoe@tertium.com",
                       "Pluto Inc.",
                       "123 Letha Shore.",
                       "",
                       "Dis", "CA",
                       "91222",
                       "US",
                       "12345678910");
  std::vector<base::string16> labels;
  // Two fields at least - no filter.
  AutofillProfile::CreateInferredLabels(profiles.get(), NULL, UNKNOWN_TYPE, 2,
                                        &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);

  // Three fields at least - no filter.
  AutofillProfile::CreateInferredLabels(profiles.get(), NULL, UNKNOWN_TYPE, 3,
                                        &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., Elysium"),
            labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore., Dis"),
            labels[1]);

  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(ADDRESS_HOME_CITY);
  suggested_fields.push_back(ADDRESS_HOME_STATE);
  suggested_fields.push_back(ADDRESS_HOME_ZIP);

  // Two fields at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        UNKNOWN_TYPE, 2, &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, CA"), labels[1]);

  // Three fields at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        UNKNOWN_TYPE, 3, &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, CA, 91111"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, CA, 91222"), labels[1]);

  // Three fields at least, from suggested fields - but filter reduces available
  // fields to two.
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        ADDRESS_HOME_STATE, 3, &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, 91111"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, 91222"), labels[1]);

  suggested_fields.clear();
  // In our implementation we always display NAME_FULL for all NAME* fields...
  suggested_fields.push_back(NAME_MIDDLE);
  // One field at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        UNKNOWN_TYPE, 1, &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe"), labels[1]);

  // One field at least, from suggested fields - filter the same as suggested
  // field.
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        NAME_MIDDLE, 1, &labels);
  EXPECT_EQ(base::string16(), labels[0]);
  EXPECT_EQ(base::string16(), labels[1]);

  suggested_fields.clear();
  // In our implementation we always display NAME_FULL for NAME_MIDDLE_INITIAL
  suggested_fields.push_back(NAME_MIDDLE_INITIAL);
  // One field at least, from suggested fields - no filter.
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        UNKNOWN_TYPE, 1, &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe"), labels[1]);

  // One field at least, from suggested fields - filter same as the first non-
  // unknown suggested field.
  suggested_fields.clear();
  suggested_fields.push_back(UNKNOWN_TYPE);
  suggested_fields.push_back(NAME_FULL);
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        NAME_FULL, 1, &labels);
  EXPECT_EQ(base::string16(ASCIIToUTF16("666 Erebus St.")), labels[0]);
  EXPECT_EQ(base::string16(ASCIIToUTF16("123 Letha Shore.")), labels[1]);
}

// Test that we fall back to using the full name if there are no other
// distinguishing fields, but only if it makes sense given the suggested fields.
TEST(AutofillProfileTest, CreateInferredLabelsFallsBackToFullName) {
  ScopedVector<AutofillProfile> profiles;
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[0],
                       "John", "", "Doe", "doe@example.com", "",
                       "88 Nowhere Ave.", "", "", "", "", "", "");
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[1],
                       "Johnny", "K", "Doe", "doe@example.com", "",
                       "88 Nowhere Ave.", "", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(NAME_LAST);
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  suggested_fields.push_back(EMAIL_ADDRESS);
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        NAME_LAST, 1, &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave."), labels[1]);

  // Otherwise, we should.
  suggested_fields.push_back(NAME_FIRST);
  AutofillProfile::CreateInferredLabels(profiles.get(),  &suggested_fields,
                                        NAME_LAST, 1, &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., Johnny K Doe"), labels[1]);
}

// Test that we do not show duplicate fields in the labels.
TEST(AutofillProfileTest, CreateInferredLabelsNoDuplicatedFields) {
  ScopedVector<AutofillProfile> profiles;
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[0],
                       "John", "", "Doe", "doe@example.com", "",
                       "88 Nowhere Ave.", "", "", "", "", "", "");
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[1],
                       "John", "", "Doe", "dojo@example.com", "",
                       "88 Nowhere Ave.", "", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  suggested_fields.push_back(ADDRESS_BILLING_LINE1);
  suggested_fields.push_back(EMAIL_ADDRESS);
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        UNKNOWN_TYPE, 2, &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., doe@example.com"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., dojo@example.com"), labels[1]);
}

// Make sure that empty fields are not treated as distinguishing fields.
TEST(AutofillProfileTest, CreateInferredLabelsSkipsEmptyFields) {
  ScopedVector<AutofillProfile> profiles;
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[0],
                       "John", "", "Doe", "doe@example.com",
                       "Gogole", "", "", "", "", "", "", "");
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[1],
                       "John", "", "Doe", "doe@example.com",
                       "Ggoole", "", "", "", "", "", "", "");
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[2],
                       "John", "", "Doe", "john.doe@example.com",
                       "Goolge", "", "", "", "", "", "", "");

  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(profiles.get(), NULL, UNKNOWN_TYPE, 3,
                                        &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Gogole"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Ggoole"), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, john.doe@example.com, Goolge"), labels[2]);

  // A field must have a non-empty value for each profile to be considered a
  // distinguishing field.
  profiles[1]->SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("88 Nowhere Ave."));
  AutofillProfile::CreateInferredLabels(profiles.get(), NULL, UNKNOWN_TYPE, 1,
                                        &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Gogole"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 88 Nowhere Ave., doe@example.com, Ggoole"),
            labels[1]) << labels[1];
  EXPECT_EQ(ASCIIToUTF16("John Doe, john.doe@example.com"), labels[2]);
}

// Test that labels that would otherwise have multiline values are flattened.
TEST(AutofillProfileTest, CreateInferredLabelsFlattensMultiLineValues) {
  ScopedVector<AutofillProfile> profiles;
  profiles.push_back(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(profiles[0],
                       "John", "", "Doe", "doe@example.com", "",
                       "88 Nowhere Ave.", "Apt. 42", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<ServerFieldType> suggested_fields;
  suggested_fields.push_back(NAME_FULL);
  suggested_fields.push_back(ADDRESS_HOME_STREET_ADDRESS);
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(profiles.get(), &suggested_fields,
                                        NAME_FULL, 1, &labels);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., Apt. 42"), labels[0]);
}

TEST(AutofillProfileTest, IsSubsetOf) {
  scoped_ptr<AutofillProfile> a, b;

  // |a| is a subset of |b|.
  a.reset(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  b.reset(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(a.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL);
  test::SetProfileInfo(b.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", "United States Government", "Monticello",
      NULL, "Charlottesville", "Virginia", "22902", NULL, NULL);
  EXPECT_TRUE(a->IsSubsetOf(*b, "en-US"));

  // |b| is not a subset of |a|.
  EXPECT_FALSE(b->IsSubsetOf(*a, "en-US"));

  // |a| is a subset of |a|.
  EXPECT_TRUE(a->IsSubsetOf(*a, "en-US"));

  // One field in |b| is different.
  a.reset(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  b.reset(
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com/"));
  test::SetProfileInfo(a.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL);
  test::SetProfileInfo(a.get(), "Thomas", NULL, "Adams",
      "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL);
  EXPECT_FALSE(a->IsSubsetOf(*b, "en-US"));
}

TEST(AutofillProfileTest, OverwriteWithOrAddTo) {
  AutofillProfile a(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US",
                       "12345678910");
  std::vector<base::string16> names;
  a.GetRawMultiInfo(NAME_FULL, &names);
  names.push_back(ASCIIToUTF16("Marion Morrison"));
  a.SetRawMultiInfo(NAME_FULL, names);

  // Create an identical profile except that the new profile:
  //   (1) Has a different origin,
  //   (2) Has a different address line 2,
  //   (3) Lacks a company name,
  //   (4) Has a different full name variant, and
  //   (5) Has a language code.
  AutofillProfile b = a;
  b.set_guid(base::GenerateGUID());
  b.set_origin("Chrome settings");
  b.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("area 51"));
  b.SetRawInfo(COMPANY_NAME, base::string16());
  b.GetRawMultiInfo(NAME_FULL, &names);
  names.push_back(ASCIIToUTF16("Marion M. Morrison"));
  b.SetRawMultiInfo(NAME_FULL, names);
  b.set_language_code("en");

  a.OverwriteWithOrAddTo(b, "en-US");
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(ASCIIToUTF16("area 51"), a.GetRawInfo(ADDRESS_HOME_LINE2));
  EXPECT_EQ(ASCIIToUTF16("Fox"), a.GetRawInfo(COMPANY_NAME));
  a.GetRawMultiInfo(NAME_FULL, &names);
  ASSERT_EQ(3U, names.size());
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison"), names[0]);
  EXPECT_EQ(ASCIIToUTF16("Marion Morrison"), names[1]);
  EXPECT_EQ(ASCIIToUTF16("Marion M. Morrison"), names[2]);
  EXPECT_EQ("en", a.language_code());
}

TEST(AutofillProfileTest, AssignmentOperator) {
  AutofillProfile a(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US",
                       "12345678910");

  // Result of assignment should be logically equal to the original profile.
  AutofillProfile b(base::GenerateGUID(), "http://www.example.com/");
  b = a;
  EXPECT_TRUE(a == b);

  // Assignment to self should not change the profile value.
  a = a;
  EXPECT_TRUE(a == b);
}

TEST(AutofillProfileTest, Copy) {
  AutofillProfile a(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison",
                       "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US",
                       "12345678910");

  // Clone should be logically equal to the original.
  AutofillProfile b(a);
  EXPECT_TRUE(a == b);
}

TEST(AutofillProfileTest, Compare) {
  AutofillProfile a(base::GenerateGUID(), std::string());
  AutofillProfile b(base::GenerateGUID(), std::string());

  // Empty profiles are the same.
  EXPECT_EQ(0, a.Compare(b));

  // GUIDs don't count.
  a.set_guid(base::GenerateGUID());
  b.set_guid(base::GenerateGUID());
  EXPECT_EQ(0, a.Compare(b));

  // Origins don't count.
  a.set_origin("apple");
  b.set_origin("banana");
  EXPECT_EQ(0, a.Compare(b));

  // Different values produce non-zero results.
  test::SetProfileInfo(&a, "Jimmy", NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  test::SetProfileInfo(&b, "Ringo", NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));

  // Phone numbers are compared by the full number, including the area code.
  // This is a regression test for http://crbug.com/163024
  test::SetProfileInfo(&a, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, "650.555.4321");
  test::SetProfileInfo(&b, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, "408.555.4321");
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));
}

TEST(AutofillProfileTest, MultiValueNames) {
  AutofillProfile p(base::GenerateGUID(), "https://www.example.com/");
  const base::string16 kJohnDoe(ASCIIToUTF16("John Doe"));
  const base::string16 kJohnPDoe(ASCIIToUTF16("John P. Doe"));
  std::vector<base::string16> set_values;
  set_values.push_back(kJohnDoe);
  set_values.push_back(kJohnPDoe);
  p.SetRawMultiInfo(NAME_FULL, set_values);

  // Expect regular |GetInfo| returns the first element.
  EXPECT_EQ(kJohnDoe, p.GetRawInfo(NAME_FULL));

  // Ensure that we get out what we put in.
  std::vector<base::string16> get_values;
  p.GetRawMultiInfo(NAME_FULL, &get_values);
  ASSERT_EQ(2UL, get_values.size());
  EXPECT_EQ(kJohnDoe, get_values[0]);
  EXPECT_EQ(kJohnPDoe, get_values[1]);

  // Update the values.
  AutofillProfile p2 = p;
  EXPECT_EQ(0, p.Compare(p2));
  const base::string16 kNoOne(ASCIIToUTF16("No One"));
  set_values[1] = kNoOne;
  p.SetRawMultiInfo(NAME_FULL, set_values);
  p.GetRawMultiInfo(NAME_FULL, &get_values);
  ASSERT_EQ(2UL, get_values.size());
  EXPECT_EQ(kJohnDoe, get_values[0]);
  EXPECT_EQ(kNoOne, get_values[1]);
  EXPECT_NE(0, p.Compare(p2));

  // Delete values.
  set_values.clear();
  p.SetRawMultiInfo(NAME_FULL, set_values);
  p.GetRawMultiInfo(NAME_FULL, &get_values);
  ASSERT_EQ(1UL, get_values.size());
  EXPECT_EQ(base::string16(), get_values[0]);

  // Expect regular |GetInfo| returns empty value.
  EXPECT_EQ(base::string16(), p.GetRawInfo(NAME_FULL));
}

TEST(AutofillProfileTest, MultiValueEmails) {
  AutofillProfile p(base::GenerateGUID(), "https://www.example.com/");
  const base::string16 kJohnDoe(ASCIIToUTF16("john@doe.com"));
  const base::string16 kJohnPDoe(ASCIIToUTF16("john_p@doe.com"));
  std::vector<base::string16> set_values;
  set_values.push_back(kJohnDoe);
  set_values.push_back(kJohnPDoe);
  p.SetRawMultiInfo(EMAIL_ADDRESS, set_values);

  // Expect regular |GetInfo| returns the first element.
  EXPECT_EQ(kJohnDoe, p.GetRawInfo(EMAIL_ADDRESS));

  // Ensure that we get out what we put in.
  std::vector<base::string16> get_values;
  p.GetRawMultiInfo(EMAIL_ADDRESS, &get_values);
  ASSERT_EQ(2UL, get_values.size());
  EXPECT_EQ(kJohnDoe, get_values[0]);
  EXPECT_EQ(kJohnPDoe, get_values[1]);

  // Update the values.
  AutofillProfile p2 = p;
  EXPECT_EQ(0, p.Compare(p2));
  const base::string16 kNoOne(ASCIIToUTF16("no@one.com"));
  set_values[1] = kNoOne;
  p.SetRawMultiInfo(EMAIL_ADDRESS, set_values);
  p.GetRawMultiInfo(EMAIL_ADDRESS, &get_values);
  ASSERT_EQ(2UL, get_values.size());
  EXPECT_EQ(kJohnDoe, get_values[0]);
  EXPECT_EQ(kNoOne, get_values[1]);
  EXPECT_NE(0, p.Compare(p2));

  // Delete values.
  set_values.clear();
  p.SetRawMultiInfo(EMAIL_ADDRESS, set_values);
  p.GetRawMultiInfo(EMAIL_ADDRESS, &get_values);
  ASSERT_EQ(1UL, get_values.size());
  EXPECT_EQ(base::string16(), get_values[0]);

  // Expect regular |GetInfo| returns empty value.
  EXPECT_EQ(base::string16(), p.GetRawInfo(EMAIL_ADDRESS));
}

TEST(AutofillProfileTest, MultiValuePhone) {
  AutofillProfile p(base::GenerateGUID(), "https://www.example.com/");
  const base::string16 kJohnDoe(ASCIIToUTF16("4151112222"));
  const base::string16 kJohnPDoe(ASCIIToUTF16("4151113333"));
  std::vector<base::string16> set_values;
  set_values.push_back(kJohnDoe);
  set_values.push_back(kJohnPDoe);
  p.SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, set_values);

  // Expect regular |GetInfo| returns the first element.
  EXPECT_EQ(kJohnDoe, p.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  // Ensure that we get out what we put in.
  std::vector<base::string16> get_values;
  p.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &get_values);
  ASSERT_EQ(2UL, get_values.size());
  EXPECT_EQ(kJohnDoe, get_values[0]);
  EXPECT_EQ(kJohnPDoe, get_values[1]);

  // Update the values.
  AutofillProfile p2 = p;
  EXPECT_EQ(0, p.Compare(p2));
  const base::string16 kNoOne(ASCIIToUTF16("4152110000"));
  set_values[1] = kNoOne;
  p.SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, set_values);
  p.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &get_values);
  ASSERT_EQ(2UL, get_values.size());
  EXPECT_EQ(kJohnDoe, get_values[0]);
  EXPECT_EQ(kNoOne, get_values[1]);
  EXPECT_NE(0, p.Compare(p2));

  // Delete values.
  set_values.clear();
  p.SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, set_values);
  p.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &get_values);
  ASSERT_EQ(1UL, get_values.size());
  EXPECT_EQ(base::string16(), get_values[0]);

  // Expect regular |GetInfo| returns empty value.
  EXPECT_EQ(base::string16(), p.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
}

TEST(AutofillProfileTest, IsPresentButInvalid) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("C"));
  EXPECT_TRUE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));

  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_STATE));

  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("90"));
  EXPECT_TRUE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));

  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("90210"));
  EXPECT_FALSE(profile.IsPresentButInvalid(ADDRESS_HOME_ZIP));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("310"));
  EXPECT_TRUE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));

  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("(310) 310-6000"));
  EXPECT_FALSE(profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER));
}

TEST(AutofillProfileTest, SetRawInfoPreservesLineBreaks) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");

  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     ASCIIToUTF16("123 Super St.\n"
                                  "Apt. #42"));
  EXPECT_EQ(ASCIIToUTF16("123 Super St.\n"
                         "Apt. #42"),
            profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST(AutofillProfileTest, SetInfoPreservesLineBreaks) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");

  profile.SetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS),
                  ASCIIToUTF16("123 Super St.\n"
                               "Apt. #42"),
                  "en-US");
  EXPECT_EQ(ASCIIToUTF16("123 Super St.\n"
                         "Apt. #42"),
            profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
}

TEST(AutofillProfileTest, SetRawInfoDoesntTrimWhitespace) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");

  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("\tuser@example.com    "));
  EXPECT_EQ(ASCIIToUTF16("\tuser@example.com    "),
            profile.GetRawInfo(EMAIL_ADDRESS));
}

TEST(AutofillProfileTest, SetInfoTrimsWhitespace) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");

  profile.SetInfo(AutofillType(EMAIL_ADDRESS),
                  ASCIIToUTF16("\tuser@example.com    "),
                  "en-US");
  EXPECT_EQ(ASCIIToUTF16("user@example.com"),
            profile.GetRawInfo(EMAIL_ADDRESS));
}

}  // namespace autofill
