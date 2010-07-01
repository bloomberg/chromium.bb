// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests different possibilities for summary string generation.
// Based on existence of first name, last name, and address line 1.
TEST(AutoFillProfileTest, PreviewSummaryString) {
  // Case 0/null: ""
  AutoFillProfile profile0(string16(), 0);
  string16 summary0 = profile0.PreviewSummary();
  EXPECT_EQ(string16(), summary0);

  // Case 0/empty: ""
  AutoFillProfile profile00(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile00, "Billing", "", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA", "91601", "US",
      "12345678910", "01987654321");
  string16 summary00 = profile00.PreviewSummary();
  EXPECT_EQ(string16(), summary00);

  // Case 1: "<address>"
  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile1, "Billing", "", "Mitchell", "",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  string16 summary1 = profile1.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("123 Zoo St.")), summary1);

  // Case 2: "<lastname>"
  AutoFillProfile profile2(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile2, "Billing", "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  string16 summary2 = profile2.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Morrison")), summary2);

  // Case 3: "<lastname>, <address>"
  AutoFillProfile profile3(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile3, "Billing", "", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910", "01987654321");
  string16 summary3 = profile3.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Morrison, 123 Zoo St.")), summary3);

  // Case 4: "<firstname>"
  AutoFillProfile profile4(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile4, "Billing", "Marion", "Mitchell",
      "", "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA", "91601",
      "US", "12345678910", "01987654321");
  string16 summary4 = profile4.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion")), summary4);

  // Case 5: "<firstname>, <address>"
  AutoFillProfile profile5(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile5, "Billing", "Marion", "Mitchell",
      "", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  string16 summary5 = profile5.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion, 123 Zoo St.")), summary5);

  // Case 6: "<firstname> <lastname>"
  AutoFillProfile profile6(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile6, "Billing", "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");
  string16 summary6 = profile6.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion Morrison")), summary6);

  // Case 7: "<firstname> <lastname>, <address>"
  AutoFillProfile profile7(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile7, "Billing", "Marion", "Mitchell",
      "Morrison", "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA", "91601", "US", "12345678910", "01987654321");
  string16 summary7 = profile7.PreviewSummary();
  EXPECT_EQ(string16(ASCIIToUTF16("Marion Morrison, 123 Zoo St.")), summary7);
}

TEST(AutoFillProfileTest, IsSubsetOf) {
  scoped_ptr<AutoFillProfile> a, b;

  // |a| is a subset of |b|.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_unittest::SetProfileInfo(a.get(), "label1", "Thomas", NULL,
      "Jefferson", "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL);
  autofill_unittest::SetProfileInfo(b.get(), "label2", "Thomas", NULL,
      "Jefferson", "declaration_guy@gmail.com", "United States Government",
      "Monticello", NULL, "Charlottesville", "Virginia", "22902", NULL, NULL,
      NULL);
  EXPECT_TRUE(a->IsSubsetOf(*b));

  // |b| is not a subset of |a|.
  EXPECT_FALSE(b->IsSubsetOf(*a));

  // |a| is a subset of |a|.
  EXPECT_TRUE(a->IsSubsetOf(*a));

  // One field in |b| is different.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_unittest::SetProfileInfo(a.get(), "label1", "Thomas", NULL,
      "Jefferson", "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL);
  autofill_unittest::SetProfileInfo(a.get(), "label2", "Thomas", NULL,
      "Adams", "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL);
  EXPECT_FALSE(a->IsSubsetOf(*b));
}

TEST(AutoFillProfileTest, IntersectionOfTypesHasEqualValues) {
  scoped_ptr<AutoFillProfile> a, b;

  // Intersection of types contains the fields NAME_FIRST, NAME_LAST,
  // EMAIL_ADDRESS.  The values of these field types are equal between the two
  // profiles.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_unittest::SetProfileInfo(a.get(), "label1", "Thomas", NULL,
      "Jefferson", "declaration_guy@gmail.com", NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, "12134759123", "19384284720");
  autofill_unittest::SetProfileInfo(b.get(), "label2", "Thomas", NULL,
      "Jefferson", "declaration_guy@gmail.com", "United States Government",
      "Monticello", NULL, "Charlottesville", "Virginia", "22902", NULL, NULL,
      NULL);
  EXPECT_TRUE(a->IntersectionOfTypesHasEqualValues(*b));

  // Intersection of types contains the fields NAME_FIRST, NAME_LAST,
  // EMAIL_ADDRESS. The value of EMAIL_ADDRESS differs between the two profiles.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_unittest::SetProfileInfo(a.get(), "label1", "Thomas", NULL,
      "Jefferson", "poser@yahoo.com", NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, "12134759123", "19384284720");
  autofill_unittest::SetProfileInfo(b.get(), "label2", "Thomas", NULL,
      "Jefferson", "declaration_guy@gmail.com", "United States Government",
      "Monticello", NULL, "Charlottesville", "Virginia", "22902", NULL, NULL,
      NULL);
  EXPECT_FALSE(a->IntersectionOfTypesHasEqualValues(*b));

  // Intersection of types is empty.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_unittest::SetProfileInfo(a.get(), "label1", "Thomas", NULL,
      "Jefferson", "poser@yahoo.com", NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, "12134759123", "19384284720");
  autofill_unittest::SetProfileInfo(b.get(), "label2", NULL, NULL, NULL, NULL,
      "United States Government", "Monticello", NULL, "Charlottesville",
      "Virginia", "22902", NULL, NULL, NULL);
  EXPECT_FALSE(a->IntersectionOfTypesHasEqualValues(*b));
}

TEST(AutoFillProfileTest, MergeWith) {
  scoped_ptr<AutoFillProfile> a, b;

  // Merge |b| into |a|.
  a.reset(new AutoFillProfile);
  b.reset(new AutoFillProfile);
  autofill_unittest::SetProfileInfo(a.get(), "label1", "Jimmy", NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, "12134759123", "19384284720");
  autofill_unittest::SetProfileInfo(b.get(), "label2", "James", NULL,
      "Madison", "constitutionalist@gmail.com", "United States Government",
      "Monticello", NULL, "Charlottesville", "Virginia", "22902", NULL, NULL,
      NULL);
  AutoFillProfile expected_b(*b);
  a->MergeWith(*b);

  AutoFillProfile expected_a;
  autofill_unittest::SetProfileInfo(&expected_a, "label1", "Jimmy", NULL,
      "Madison", "constitutionalist@gmail.com", "United States Government",
      "Monticello", NULL, "Charlottesville", "Virginia", "22902", NULL,
      "12134759123", "19384284720");
  EXPECT_EQ(expected_a, *a);
  EXPECT_EQ(expected_b, *b);
}

}  // namespace

