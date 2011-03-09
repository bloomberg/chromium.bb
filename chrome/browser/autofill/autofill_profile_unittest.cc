// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/common/guid.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool UpdateProfileLabel(AutoFillProfile *profile) {
  std::vector<AutoFillProfile*> profiles;
  profiles.push_back(profile);
  return AutoFillProfile::AdjustInferredLabels(&profiles);
}

}  // namespace

// Tests different possibilities for summary string generation.
// Based on existence of first name, last name, and address line 1.
TEST(AutoFillProfileTest, PreviewSummaryString) {
  // Case 0/null: ""
  AutoFillProfile profile0;
  // Empty profile - nothing to update.
  EXPECT_FALSE(UpdateProfileLabel(&profile0));
  string16 summary0 = profile0.Label();
  EXPECT_EQ(string16(), summary0);

  // Case 0a/empty name and address, so the first two fields of the rest of the
  // data is used: "Hollywood, CA"
  AutoFillProfile profile00;
  autofill_test::SetProfileInfo(&profile00, "", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA", "91601", "US",
      "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile00));
  string16 summary00 = profile00.Label();
  EXPECT_EQ(ASCIIToUTF16("Hollywood, CA"), summary00);

  // Case 1: "<address>"
  AutoFillProfile profile1;
  autofill_test::SetProfileInfo(&profile1, "", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile1));
  string16 summary1 = profile1.Label();
  EXPECT_EQ(ASCIIToUTF16("123 Zoo St., Hollywood"), summary1);

  // Case 2: "<lastname>"
  AutoFillProfile profile2;
  autofill_test::SetProfileInfo(&profile2, "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile2));
  string16 summary2 = profile2.Label();
  // Summary does include full name which is empty if the first name is empty.
  EXPECT_EQ(ASCIIToUTF16("Hollywood, CA"), summary2);

  // Case 3: "<lastname>, <address>"
  AutoFillProfile profile3;
  autofill_test::SetProfileInfo(&profile3, "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile3));
  string16 summary3 = profile3.Label();
  EXPECT_EQ(ASCIIToUTF16("123 Zoo St., Hollywood"), summary3);

  // Case 4: "<firstname>"
  AutoFillProfile profile4;
  autofill_test::SetProfileInfo(&profile4, "Marion", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA", "91601", "US",
      "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile4));
  string16 summary4 = profile4.Label();
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell, Hollywood"), summary4);

  // Case 5: "<firstname>, <address>"
  AutoFillProfile profile5;
  autofill_test::SetProfileInfo(&profile5, "Marion", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile5));
  string16 summary5 = profile5.Label();
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell, 123 Zoo St."), summary5);

  // Case 6: "<firstname> <lastname>"
  AutoFillProfile profile6;
  autofill_test::SetProfileInfo(&profile6, "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile6));
  string16 summary6 = profile6.Label();
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison, Hollywood"),
            summary6);

  // Case 7: "<firstname> <lastname>, <address>"
  AutoFillProfile profile7;
  autofill_test::SetProfileInfo(&profile7, "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910", "01987654321");
  EXPECT_TRUE(UpdateProfileLabel(&profile7));
  string16 summary7 = profile7.Label();
  EXPECT_EQ(ASCIIToUTF16("Marion Mitchell Morrison, 123 Zoo St."),
            summary7);

  // Case 7a: "<firstname> <lastname>, <address>" - same as #7, except for
  // e-mail.
  AutoFillProfile profile7a;
  autofill_test::SetProfileInfo(&profile7a, "Marion", "Mitchell",
    "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
    "Hollywood", "CA", "91601", "US", "12345678910", "01987654321");
  std::vector<AutoFillProfile*> profiles;
  profiles.push_back(&profile7);
  profiles.push_back(&profile7a);
  EXPECT_TRUE(AutoFillProfile::AdjustInferredLabels(&profiles));
  summary7 = profile7.Label();
  string16 summary7a = profile7a.Label();
  EXPECT_EQ(ASCIIToUTF16(
      "Marion Mitchell Morrison, 123 Zoo St., johnwayne@me.xyz"), summary7);
  EXPECT_EQ(ASCIIToUTF16(
      "Marion Mitchell Morrison, 123 Zoo St., marion@me.xyz"), summary7a);
}

TEST(AutoFillProfileTest, AdjustInferredLabels) {
  std::vector<AutoFillProfile*> profiles;
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(
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
      "11111111111",
      "22222222222");
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(
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
      "12345678910",
      "01987654321");
  // As labels are empty they are adjusted the first time.
  EXPECT_TRUE(AutoFillProfile::AdjustInferredLabels(&profiles));
  // No need to adjust them anymore.
  EXPECT_FALSE(AutoFillProfile::AdjustInferredLabels(&profiles));
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St."),
            profiles[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."),
            profiles[1]->Label());

  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(
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
      "11111111111",
      "22222222222");
  EXPECT_TRUE(AutoFillProfile::AdjustInferredLabels(&profiles));

  // Profile 0 and 2 inferred label now includes an e-mail.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@hades.com"),
            profiles[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."),
            profiles[1]->Label());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@tertium.com"),
            profiles[2]->Label());

  delete profiles[2];
  profiles.pop_back();

  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(
      profiles[2],
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
      "11111111111",
      "33333333333");  // Fax is different

  EXPECT_TRUE(AutoFillProfile::AdjustInferredLabels(&profiles));

  // Profile 0 and 2 inferred label now includes a fax number.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., fax:#22222222222"),
            profiles[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."),
            profiles[1]->Label());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., fax:#33333333333"),
            profiles[2]->Label());

  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(
      profiles[3],
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
      "44444444444",  // Phone is different for some.
      "33333333333");  // Fax is different for some.

  EXPECT_TRUE(AutoFillProfile::AdjustInferredLabels(&profiles));

  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., 11111111111,"
                         " fax:#22222222222"),
            profiles[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."),
            profiles[1]->Label());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., 11111111111,"
                         " fax:#33333333333"),
            profiles[2]->Label());
  // This one differs from other ones by unique phone, so no need for extra
  // information.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., 44444444444"),
            profiles[3]->Label());

  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(
      profiles[4],
      "John",
      "",
      "Doe",
      "johndoe@styx.com",  // E-Mail is different for some.
      "Underworld",
      "666 Erebus St.",
      "",
      "Elysium", "CA",
      "91111",
      "US",
      "44444444444",  // Phone is different for some.
      "33333333333");  // Fax is different for some.

  EXPECT_TRUE(AutoFillProfile::AdjustInferredLabels(&profiles));

  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@hades.com,"
                         " 11111111111, fax:#22222222222"),
            profiles[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."),
            profiles[1]->Label());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@hades.com,"
                         " 11111111111, fax:#33333333333"),
            profiles[2]->Label());
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@hades.com,"
                         " 44444444444, fax:#33333333333"),
            profiles[3]->Label());
  // This one differs from other ones by unique e-mail, so no need for extra
  // information.
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., johndoe@styx.com"),
            profiles[4]->Label());

  EXPECT_FALSE(AutoFillProfile::AdjustInferredLabels(&profiles));

  // Clean up.
  STLDeleteContainerPointers(profiles.begin(), profiles.end());
}

TEST(AutoFillProfileTest, CreateInferredLabels) {
  std::vector<AutoFillProfile*> profiles;
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[0],
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
                                "11111111111",
                                "22222222222");
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[1],
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
                                "12345678910",
                                "01987654321");
  std::vector<string16> labels;
  // Two fields at least - no filter.
  AutoFillProfile::CreateInferredLabels(&profiles, NULL, UNKNOWN_TYPE, 2,
                                        &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore."), labels[1]);

  // Three fields at least - no filter.
  AutoFillProfile::CreateInferredLabels(&profiles, NULL, UNKNOWN_TYPE, 3,
                                        &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 666 Erebus St., Elysium"),
            labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe, 123 Letha Shore., Dis"),
            labels[1]);

  std::vector<AutofillFieldType> suggested_fields;
  suggested_fields.push_back(ADDRESS_HOME_CITY);
  suggested_fields.push_back(ADDRESS_HOME_STATE);
  suggested_fields.push_back(ADDRESS_HOME_ZIP);

  // Two fields at least, from suggested fields - no filter.
  AutoFillProfile::CreateInferredLabels(&profiles, &suggested_fields,
                                        UNKNOWN_TYPE, 2, &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, CA"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, CA"), labels[1]);

  // Three fields at least, from suggested fields - no filter.
  AutoFillProfile::CreateInferredLabels(&profiles, &suggested_fields,
                                        UNKNOWN_TYPE, 3, &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, CA, 91111"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, CA, 91222"), labels[1]);

  // Three fields at least, from suggested fields - but filter reduces available
  // fields to two.
  AutoFillProfile::CreateInferredLabels(&profiles, &suggested_fields,
                                        ADDRESS_HOME_STATE, 3, &labels);
  EXPECT_EQ(ASCIIToUTF16("Elysium, 91111"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Dis, 91222"), labels[1]);

  suggested_fields.clear();
  // In our implementation we always display NAME_FULL for all NAME* fields...
  suggested_fields.push_back(NAME_MIDDLE);
  // One field at least, from suggested fields - no filter.
  AutoFillProfile::CreateInferredLabels(&profiles, &suggested_fields,
                                        UNKNOWN_TYPE, 1, &labels);
  EXPECT_EQ(ASCIIToUTF16("John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Jane Doe"), labels[1]);

  // One field at least, from suggested fields - filter the same as suggested
  // field.
  AutoFillProfile::CreateInferredLabels(&profiles, &suggested_fields,
                                        NAME_MIDDLE, 1, &labels);
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);

  // One field at least, from suggested fields - filter same as the first non-
  // unknown suggested field.
  suggested_fields.clear();
  suggested_fields.push_back(UNKNOWN_TYPE);
  suggested_fields.push_back(NAME_FULL);
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  AutoFillProfile::CreateInferredLabels(&profiles, &suggested_fields, NAME_FULL,
                                        1, &labels);
  EXPECT_EQ(string16(ASCIIToUTF16("666 Erebus St.")), labels[0]);
  EXPECT_EQ(string16(ASCIIToUTF16("123 Letha Shore.")), labels[1]);

  // Clean up.
  STLDeleteContainerPointers(profiles.begin(), profiles.end());
}

// Test that we fall back to using the full name if there are no other
// distinguishing fields, but only if it makes sense given the suggested fields.
TEST(AutoFillProfileTest, CreateInferredLabelsFallsBackToFullName) {
  ScopedVector<AutoFillProfile> profiles;
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[0],
                                "John", "", "Doe", "doe@example.com", "",
                                "88 Nowhere Ave.", "", "", "", "", "", "", "");
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[1],
                                "Johnny", "K", "Doe", "doe@example.com", "",
                                "88 Nowhere Ave.", "", "", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<AutofillFieldType> suggested_fields;
  suggested_fields.push_back(NAME_LAST);
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  suggested_fields.push_back(EMAIL_ADDRESS);
  std::vector<string16> labels;
  AutoFillProfile::CreateInferredLabels(&profiles.get(), &suggested_fields,
                                        NAME_LAST, 1, &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave."), labels[1]);

  // Otherwise, we should.
  suggested_fields.push_back(NAME_FIRST);
  AutoFillProfile::CreateInferredLabels(&profiles.get(),  &suggested_fields,
                                        NAME_LAST, 1, &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., John Doe"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., Johnny K Doe"), labels[1]);
}

// Test that we do not show duplicate fields in the labels.
TEST(AutoFillProfileTest, CreateInferredLabelsNoDuplicatedFields) {
  ScopedVector<AutoFillProfile> profiles;
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[0],
                                "John", "", "Doe", "doe@example.com", "",
                                "88 Nowhere Ave.", "", "", "", "", "", "", "");
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[1],
                                "John", "", "Doe", "dojo@example.com", "",
                                "88 Nowhere Ave.", "", "", "", "", "", "", "");

  // If the only name field in the suggested fields is the excluded field, we
  // should not fall back to the full name as a distinguishing field.
  std::vector<AutofillFieldType> suggested_fields;
  suggested_fields.push_back(ADDRESS_HOME_LINE1);
  suggested_fields.push_back(ADDRESS_BILLING_LINE1);
  suggested_fields.push_back(EMAIL_ADDRESS);
  std::vector<string16> labels;
  AutoFillProfile::CreateInferredLabels(&profiles.get(), &suggested_fields,
                                        UNKNOWN_TYPE, 2, &labels);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., doe@example.com"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("88 Nowhere Ave., dojo@example.com"), labels[1]);
}

// Make sure that empty fields are not treated as distinguishing fields.
TEST(AutoFillProfileTest, CreateInferredLabelsSkipsEmptyFields) {
  ScopedVector<AutoFillProfile> profiles;
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[0],
                                "John", "", "Doe", "doe@example.com",
                                "Gogole", "", "", "", "", "", "", "", "");
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[1],
                                "John", "", "Doe", "doe@example.com",
                                "Ggoole", "", "", "", "", "", "", "", "");
  profiles.push_back(new AutoFillProfile);
  autofill_test::SetProfileInfo(profiles[2],
                                "John", "", "Doe", "john.doe@example.com",
                                "Goolge", "", "", "", "", "", "", "", "");

  std::vector<string16> labels;
  AutoFillProfile::CreateInferredLabels(&profiles.get(), NULL, UNKNOWN_TYPE, 3,
                                        &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Gogole"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Ggoole"), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, john.doe@example.com, Goolge"), labels[2]);

  // A field must have a non-empty value for each profile to be considered a
  // distinguishing field.
  profiles[1]->SetInfo(AutofillType(ADDRESS_HOME_LINE1),
                       ASCIIToUTF16("88 Nowhere Ave."));
  AutoFillProfile::CreateInferredLabels(&profiles.get(), NULL, UNKNOWN_TYPE, 1,
                                        &labels);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("John Doe, doe@example.com, Gogole"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("John Doe, 88 Nowhere Ave., doe@example.com, Ggoole"),
            labels[1]) << labels[1];
  EXPECT_EQ(ASCIIToUTF16("John Doe, john.doe@example.com"), labels[2]);
}

TEST(AutoFillProfileTest, IsSubsetOf) {
  scoped_ptr<AutoFillProfile> a, b;

  // |a| is a subset of |b|.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_test::SetProfileInfo(a.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL);
  autofill_test::SetProfileInfo(b.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", "United States Government", "Monticello",
      NULL, "Charlottesville", "Virginia", "22902", NULL, NULL, NULL);
  EXPECT_TRUE(a->IsSubsetOf(*b));

  // |b| is not a subset of |a|.
  EXPECT_FALSE(b->IsSubsetOf(*a));

  // |a| is a subset of |a|.
  EXPECT_TRUE(a->IsSubsetOf(*a));

  // One field in |b| is different.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_test::SetProfileInfo(a.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL);
  autofill_test::SetProfileInfo(a.get(), "Thomas", NULL, "Adams",
      "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL);
  EXPECT_FALSE(a->IsSubsetOf(*b));
}

TEST(AutoFillProfileTest, IntersectionOfTypesHasEqualValues) {
  scoped_ptr<AutoFillProfile> a, b;

  // Intersection of types contains the fields NAME_FIRST, NAME_LAST,
  // EMAIL_ADDRESS.  The values of these field types are equal between the two
  // profiles.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_test::SetProfileInfo(a.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      "12134759123", "19384284720");
  autofill_test::SetProfileInfo(b.get(), "Thomas", NULL, "Jefferson",
      "declaration_guy@gmail.com", "United States Government", "Monticello",
      NULL, "Charlottesville", "Virginia", "22902", NULL, NULL, NULL);
  EXPECT_TRUE(a->IntersectionOfTypesHasEqualValues(*b));

  // Intersection of types contains the fields NAME_FIRST, NAME_LAST,
  // EMAIL_ADDRESS. The value of EMAIL_ADDRESS differs between the two profiles.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_test::SetProfileInfo(a.get(), "Thomas", NULL, "Jefferson",
      "poser@yahoo.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      "12134759123", "19384284720");
  autofill_test::SetProfileInfo(b.get(), "Thomas", NULL, "Jefferson",\
      "declaration_guy@gmail.com", "United States Government", "Monticello",
      NULL, "Charlottesville", "Virginia", "22902", NULL, NULL, NULL);
  EXPECT_FALSE(a->IntersectionOfTypesHasEqualValues(*b));

  // Intersection of types is empty.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_test::SetProfileInfo(a.get(), "Thomas", NULL, "Jefferson",
      "poser@yahoo.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      "12134759123", "19384284720");
  autofill_test::SetProfileInfo(b.get(), NULL, NULL, NULL, NULL,
      "United States Government", "Monticello", NULL, "Charlottesville",
      "Virginia", "22902", NULL, NULL, NULL);
  EXPECT_FALSE(a->IntersectionOfTypesHasEqualValues(*b));
}

TEST(AutoFillProfileTest, MergeWith) {
  scoped_ptr<AutoFillProfile> a, b;

  // Merge |b| into |a|.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_test::SetProfileInfo(a.get(), "Jimmy", NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, "12134759123", "19384284720");
  autofill_test::SetProfileInfo(b.get(), "James", NULL, "Madison",
      "constitutionalist@gmail.com", "United States Government", "Monticello",
      NULL, "Charlottesville", "Virginia", "22902", NULL, NULL, NULL);
  AutoFillProfile expected_b(*b);
  a->MergeWith(*b);

  AutoFillProfile expected_a;
  autofill_test::SetProfileInfo(&expected_a, "Jimmy", NULL, "Madison",
      "constitutionalist@gmail.com", "United States Government", "Monticello",
      NULL, "Charlottesville", "Virginia", "22902", NULL, "12134759123",
      "19384284720");
  EXPECT_EQ(0, expected_a.Compare(*a));
  EXPECT_EQ(0, expected_b.Compare(*b));
}

TEST(AutoFillProfileTest, AssignmentOperator){
  AutoFillProfile a, b;

  // Result of assignment should be logically equal to the original profile.
  autofill_test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison",
                                "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                                "Hollywood", "CA", "91601", "US", "12345678910",
                                "01987654321");
  b = a;
  EXPECT_TRUE(a == b);

  // Assignment to self should not change the profile value.
  a = a;
  EXPECT_TRUE(a == b);
}

TEST(AutoFillProfileTest, Copy) {
  AutoFillProfile a;

  // Clone should be logically equal to the original.
  autofill_test::SetProfileInfo(&a, "Marion", "Mitchell", "Morrison",
                                "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                                "Hollywood", "CA", "91601", "US", "12345678910",
                                "01987654321");
  AutoFillProfile b(a);
  EXPECT_TRUE(a == b);
}

TEST(AutoFillProfileTest, Compare) {
  AutoFillProfile a, b;

  // Empty profiles are the same.
  EXPECT_EQ(0, a.Compare(b));

  // GUIDs don't count.
  a.set_guid(guid::GenerateGUID());
  b.set_guid(guid::GenerateGUID());
  EXPECT_EQ(0, a.Compare(b));

  // Different values produce non-zero results.
  autofill_test::SetProfileInfo(&a, "Jimmy", NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  autofill_test::SetProfileInfo(&b, "Ringo", NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));
}

TEST(AutoFillProfileTest, CountryCode) {
  AutoFillProfile profile;
  EXPECT_EQ(std::string(), profile.CountryCode());

  profile.SetCountryCode("US");
  EXPECT_EQ("US", profile.CountryCode());
}
