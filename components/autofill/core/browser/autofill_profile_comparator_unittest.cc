// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_comparator.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::ADDRESS_HOME_CITY;
using autofill::ADDRESS_HOME_COUNTRY;
using autofill::ADDRESS_HOME_LINE1;
using autofill::ADDRESS_HOME_LINE2;
using autofill::ADDRESS_HOME_STATE;
using autofill::ADDRESS_HOME_ZIP;
using autofill::AutofillProfile;
using autofill::AutofillType;
using autofill::NAME_FIRST;
using autofill::NAME_MIDDLE;
using autofill::PHONE_HOME_WHOLE_NUMBER;
using autofill::ServerFieldType;
using base::UTF8ToUTF16;

namespace {

class AutofillProfileComparatorTest : public ::testing::Test {
 public:
  // Expose the protected methods of autofill::AutofillProfileComparator for
  // testing.
  class AutofillProfileComparator
      : public ::autofill::AutofillProfileComparator {
   public:
    typedef ::autofill::AutofillProfileComparator Super;
    using Super::Super;
    using Super::UniqueTokens;
    using Super::HaveSameTokens;
    using Super::GetNamePartVariants;
    using Super::IsNameVariantOf;
    using Super::HaveMergeableNames;
    using Super::HaveMergeableEmailAddresses;
    using Super::HaveMergeableCompanyNames;
    using Super::HaveMergeablePhoneNumbers;
    using Super::HaveMergeableAddresses;
  };

  AutofillProfileComparatorTest() : comparator_("en-US") {}

  AutofillProfile CreateProfileWithName(const char* first,
                                        const char* middle,
                                        const char* last) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, first, middle, last, "", "", "",
                                   "", "", "", "", "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithEmail(const char* email) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", email, "", "", "", "",
                                   "", "", "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithCompanyName(const char* company_name) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", "", company_name, "",
                                   "", "", "", "", "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithPhoneNumber(const char* phone_number) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "",
                                   "", "", phone_number);
    return profile;
  }

  AutofillProfile CreateProfileWithAddress(const char* line1,
                                           const char* line2,
                                           const char* city,
                                           const char* state,
                                           const char* zip,
                                           const char* country) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", "", "", line1, line2,
                                   city, state, zip, country, "");
    return profile;
  }

  AutofillProfile CopyAndModify(
      const AutofillProfile& profile,
      const std::vector<std::pair<ServerFieldType, const char*>>& updates) {
    AutofillProfile new_profile = profile;
    for (const auto& update : updates) {
      new_profile.SetRawInfo(update.first, UTF8ToUTF16(update.second));
    }
    return new_profile;
  }

  AutofillProfileComparator comparator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillProfileComparatorTest);
};

}  // namespace

TEST_F(AutofillProfileComparatorTest, UniqueTokens) {
  base::string16 kInput = UTF8ToUTF16("a b a a b");
  std::vector<base::string16> tokens = {UTF8ToUTF16("a"), UTF8ToUTF16("b")};
  EXPECT_EQ(std::set<base::StringPiece16>(tokens.begin(), tokens.end()),
            comparator_.UniqueTokens(kInput));
}

TEST_F(AutofillProfileComparatorTest, HaveSameTokens) {
  base::string16 kEmptyStr = UTF8ToUTF16("");
  base::string16 kHello = UTF8ToUTF16("hello");
  base::string16 kHelloThere = UTF8ToUTF16("hello there");
  base::string16 kHelloThereAlice = UTF8ToUTF16("hello there alice");
  base::string16 kHelloThereBob = UTF8ToUTF16("hello there bob");

  EXPECT_TRUE(comparator_.HaveSameTokens(kEmptyStr, kHello));
  EXPECT_TRUE(comparator_.HaveSameTokens(kHello, kEmptyStr));
  EXPECT_TRUE(comparator_.HaveSameTokens(kHelloThere, kHello));
  EXPECT_TRUE(comparator_.HaveSameTokens(kHello, kHelloThere));
  EXPECT_FALSE(comparator_.HaveSameTokens(kHelloThereAlice, kHelloThereBob));
  EXPECT_FALSE(comparator_.HaveSameTokens(kHelloThereBob, kHelloThereAlice));
}

TEST_F(AutofillProfileComparatorTest, NormalizeForComparison) {
  EXPECT_EQ(UTF8ToUTF16("timothe"),
            comparator_.NormalizeForComparison(UTF8ToUTF16("Timothé")));
  EXPECT_EQ(UTF8ToUTF16("sven ake"),
            comparator_.NormalizeForComparison(UTF8ToUTF16(" sven-åke ")));
  EXPECT_EQ(UTF8ToUTF16("c 㸐"),
            comparator_.NormalizeForComparison(UTF8ToUTF16("Ç 㸐")));
  EXPECT_EQ(UTF8ToUTF16("902101234"),
            comparator_.NormalizeForComparison(
                base::UTF8ToUTF16("90210-1234"),
                AutofillProfileComparator::DISCARD_WHITESPACE));
  EXPECT_EQ(UTF8ToUTF16("timothe noel etienne perier"),
            comparator_.NormalizeForComparison(
                UTF8ToUTF16("Timothé-Noël Étienne Périer")));
}

TEST_F(AutofillProfileComparatorTest, GetNamePartVariants) {
  std::set<base::string16> expected_variants = {
      UTF8ToUTF16("timothe noel"),
      UTF8ToUTF16("timothe n"),
      UTF8ToUTF16("timothe"),
      UTF8ToUTF16("t noel"),
      UTF8ToUTF16("t n"),
      UTF8ToUTF16("t"),
      UTF8ToUTF16("noel"),
      UTF8ToUTF16("n"),
      UTF8ToUTF16(""),
      UTF8ToUTF16("tn"),
  };

  EXPECT_EQ(expected_variants,
            comparator_.GetNamePartVariants(UTF8ToUTF16("timothe noel")));
}

TEST_F(AutofillProfileComparatorTest, IsNameVariantOf) {
  const base::string16 kNormalizedFullName =
      UTF8ToUTF16("timothe noel etienne perier");

  EXPECT_TRUE(
      comparator_.IsNameVariantOf(kNormalizedFullName, kNormalizedFullName));
  EXPECT_TRUE(comparator_.IsNameVariantOf(
      kNormalizedFullName, UTF8ToUTF16("t noel etienne perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("timothe perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("t perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("noel perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("t n etienne perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("tn perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("te perier")));

  EXPECT_FALSE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                           UTF8ToUTF16("etienne noel perier")));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableNames) {
  AutofillProfile empty = CreateProfileWithName("", "", "");

  AutofillProfile p1 = CreateProfileWithName("sven-åke", "", "larsson");
  AutofillProfile p2 = CreateProfileWithName("Åke", "", "Larsson");
  AutofillProfile p3 = CreateProfileWithName("A", "", "Larsson");
  AutofillProfile p4 = CreateProfileWithName("sven", "ake", "Larsson");

  AutofillProfile initials = CreateProfileWithName("SA", "", "Larsson");

  AutofillProfile different = CreateProfileWithName("Joe", "", "Larsson");

  // |p1|, |p2|, |p3|, |p4| and |empty| should all be the mergeable with
  // one another. The order of the comparands should not matter.
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p4));

  // |initials| is mergeable with |p1| and |p4| but not |p2| or |p3|.
  EXPECT_TRUE(comparator_.HaveMergeableNames(initials, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(initials, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(initials, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, initials));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, initials));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, initials));
  EXPECT_FALSE(comparator_.HaveMergeableNames(initials, p2));
  EXPECT_FALSE(comparator_.HaveMergeableNames(initials, p3));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p2, initials));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p3, initials));

  // None of the non-empty profiles should match |different|. The order of the
  // comparands should not matter.
  EXPECT_FALSE(comparator_.HaveMergeableNames(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p2, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p3, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p4, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(initials, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p1));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p2));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p3));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p4));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, initials));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableEmailAddresses) {
  AutofillProfile empty = CreateProfileWithEmail("");
  AutofillProfile p1 = CreateProfileWithEmail("FOO@BAR.COM");
  AutofillProfile p2 = CreateProfileWithEmail("foo@bar.com");
  AutofillProfile different = CreateProfileWithEmail("not@the-same.com");

  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(empty, p2));

  EXPECT_FALSE(comparator_.HaveMergeableEmailAddresses(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeableEmailAddresses(different, p1));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableCompanyNames) {
  AutofillProfile empty = CreateProfileWithCompanyName("");
  AutofillProfile p1 = CreateProfileWithCompanyName("Nestlé S.A.");
  AutofillProfile p2 = CreateProfileWithCompanyName("Nestle");
  AutofillProfile different = CreateProfileWithCompanyName("Other Corp");

  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(empty, p2));

  EXPECT_FALSE(comparator_.HaveMergeableCompanyNames(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeableCompanyNames(different, p1));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeablePhoneNumbers) {
  AutofillProfile empty = CreateProfileWithPhoneNumber("");
  AutofillProfile p1 = CreateProfileWithPhoneNumber("+1 (514) 670-8700");
  AutofillProfile p2 = CreateProfileWithPhoneNumber("514.670.8700x123");
  AutofillProfile p3 = CreateProfileWithPhoneNumber("670-8700 ext123");
  AutofillProfile p4 = CreateProfileWithPhoneNumber("6708700");
  AutofillProfile different = CreateProfileWithPhoneNumber("1-800-123-4567");

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p4));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p4));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p4));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p4));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(empty, p2));

  EXPECT_FALSE(comparator_.HaveMergeablePhoneNumbers(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeablePhoneNumbers(different, p1));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableAddresses) {
  // TODO(rogerm): Replace some of the tokens to also test the address
  // normalization string replacement rules. For example:
  //    - State/Province abbreviations
  //    - Street/St/Saint/Sainte
  //    - etc...
  AutofillProfile empty = CreateProfileWithAddress("", "", "", "", "", "");
  AutofillProfile p1 = CreateProfileWithAddress(
      "1 Some Street", "Unit 3", "City", "CA - California", "90210", "US");
  AutofillProfile p2 = CreateProfileWithAddress(
      "Unit 3", "1 Some Street", "Suburb", "california", "90 210-1234", "");
  AutofillProfile p3 =
      CreateProfileWithAddress("1 Some Street #3", "", "City", "ca", "", "us");
  AutofillProfile differentCountry =
      CopyAndModify(p1, {{ADDRESS_HOME_COUNTRY, "CA"}});
  AutofillProfile differentZip =
      CopyAndModify(p1, {{ADDRESS_HOME_ZIP, "12345"}});
  AutofillProfile differentState = CopyAndModify(
      p1, {{ADDRESS_HOME_ZIP, ""}, {ADDRESS_HOME_STATE, "Florida"}});
  AutofillProfile differentCity = CopyAndModify(
      p1, {{ADDRESS_HOME_ZIP, ""}, {ADDRESS_HOME_CITY, "Metropolis"}});
  AutofillProfile differentAddress =
      CopyAndModify(p1, {{ADDRESS_HOME_LINE1, "17 Park Lane"},
                         {ADDRESS_HOME_LINE2, "Suite 150"}});

  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(empty, p2));

  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, p3));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p3, p1));

  // |p2| and |p3| don't match because without a "real" zip match, we can't
  // resolve the mismatched city/suburb names.
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p2, p3));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p3, p2));

  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentCountry));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentZip));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentState));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentCity));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentAddress));
}

TEST_F(AutofillProfileComparatorTest, IsMergeable) {
  AutofillProfile p1 = autofill::test::GetFullProfile();
  AutofillProfile p1_mergeable = CopyAndModify(
      p1, {{NAME_MIDDLE, "Herbert"},                   // Originally "H."
           {ADDRESS_HOME_LINE1, "666 Erebus St W"}});  // Originally without W.
  AutofillProfile p1_not_mergeable =
      CopyAndModify(p1, {{NAME_FIRST, "Steven"}});  // Originally "John".
  AutofillProfile p2 = autofill::test::GetFullProfile2();

  EXPECT_TRUE(comparator_.AreMergeable(p1, p1));
  EXPECT_TRUE(comparator_.AreMergeable(p1, p1_mergeable));
  EXPECT_FALSE(comparator_.AreMergeable(p1, p1_not_mergeable));
  EXPECT_FALSE(comparator_.AreMergeable(p1, p2));
}
