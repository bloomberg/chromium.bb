// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/profile_util.h"

#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/payment_options_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillProfile;

namespace payments {
namespace profile_util {

constexpr uint32_t kRequestPayerName = 1 << 0;
constexpr uint32_t kRequestPayerEmail = 1 << 1;
constexpr uint32_t kRequestPayerPhone = 1 << 2;
constexpr uint32_t kRequestShipping = 1 << 3;

class MockPaymentOptionsProvider : public PaymentOptionsProvider {
 public:
  MockPaymentOptionsProvider(uint32_t options) : options_(options) {}

  ~MockPaymentOptionsProvider() override {}
  bool request_payer_name() const override {
    return options_ & kRequestPayerName;
  }
  bool request_payer_email() const override {
    return options_ & kRequestPayerEmail;
  }
  bool request_payer_phone() const override {
    return options_ & kRequestPayerPhone;
  }
  bool request_shipping() const override { return options_ & kRequestShipping; }
  PaymentShippingType shipping_type() const override {
    return PaymentShippingType::SHIPPING;
  }

 private:
  uint32_t options_;
};

AutofillProfile CreateProfileWithContactInfo(const char* name,
                                             const char* email,
                                             const char* phone) {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  autofill::test::SetProfileInfo(&profile, name, "", "", email, "", "", "", "",
                                 "", "", "", phone);
  return profile;
}

TEST(PaymentRequestProfileUtilTest, FilterProfilesForContact) {
  // These profiles are subset/equal, so only the first complete one is
  // included.
  AutofillProfile exclude_1 =
      CreateProfileWithContactInfo("Homer", "", "5551234567");

  AutofillProfile exclude_2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");

  AutofillProfile include_1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");

  AutofillProfile exclude_3 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");

  // This profile is different, so it should also be included. Since it is
  // less complete than |include_1|, it will appear after.
  AutofillProfile include_2 =
      CreateProfileWithContactInfo("Marge", "marge@simpson.net", "");

  // This profile is different, so it should also be included. Since it is
  // equally complete with |include_1|, it will appear before |include_2|, but
  // after |include_1| since order is preserved amongst profiles of equal
  // completeness.
  AutofillProfile include_3 = CreateProfileWithContactInfo(
      "Bart", "eatmyshorts@simpson.net", "5551234567");

  std::vector<AutofillProfile*> profiles = {&exclude_1, &exclude_2, &include_1,
                                            &exclude_3, &include_2, &include_3};

  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerEmail |
                                      kRequestPayerPhone);
  std::vector<AutofillProfile*> filtered =
      FilterProfilesForContact(profiles, "en-US", provider);

  ASSERT_EQ(3u, filtered.size());
  EXPECT_EQ(&include_1, filtered[0]);
  EXPECT_EQ(&include_3, filtered[1]);
  EXPECT_EQ(&include_2, filtered[2]);

  // Repeat the filter using a provider set to only request phone numbers.
  // Under these rules, since all profiles have the same (or no) phone number,
  // we should only see the first profile with a phone number, |exclude_1|.
  MockPaymentOptionsProvider phone_only_provider(kRequestPayerPhone);
  std::vector<AutofillProfile*> filtered_phones =
      FilterProfilesForContact(profiles, "en-US", phone_only_provider);
  ASSERT_EQ(1u, filtered_phones.size());
  EXPECT_EQ(&exclude_1, filtered_phones[0]);
}

TEST(PaymentRequestProfileUtilTest, IsContactEqualOrSuperset) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerEmail |
                                      kRequestPayerPhone);
  PaymentsProfileComparator comp("en-US", provider);

  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");

  // Candidate subset profile is equal.
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p2));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p2, p1));

  // Candidate subset profile has non-matching fields.
  AutofillProfile p3 = CreateProfileWithContactInfo(
      "Homer", "homer@springfieldnuclear.gov", "5551234567");
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p1, p3));
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p3, p1));

  // Candidate subset profile is equal, except for missing fields.
  AutofillProfile p4 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p4));
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p4, p1));

  // One field is common, but each has a field which the other is missing.
  AutofillProfile p5 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  AutofillProfile p6 = CreateProfileWithContactInfo("Homer", "", "5551234567");
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p5, p6));
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p6, p5));
}

TEST(PaymentRequestProfileUtilTest, IsContactEqualOrSuperset_WithFieldIgnored) {
  // Discrepancies in email should be ignored throughout this test.
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerPhone);
  PaymentsProfileComparator comp("en-US", provider);

  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");

  // Candidate subset profile is equal.
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p2));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p2, p1));

  // Email fields don't match, but profiles are still equal.
  AutofillProfile p3 = CreateProfileWithContactInfo(
      "Homer", "homer@springfieldnuclear.gov", "5551234567");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p3));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p3, p1));

  // Profile without an email is mutual subset of profile with an email.
  AutofillProfile p4 = CreateProfileWithContactInfo("Homer", "", "5551234567");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p4));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p4, p1));
}

TEST(PaymentRequestProfileUtilTest, GetContactCompletenessScore) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerPhone);
  PaymentsProfileComparator comp("en-US", provider);

  // Two completeness points: One each for name and phone number, but not email
  // as it was not requested.
  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");
  EXPECT_EQ(2, comp.GetContactCompletenessScore(&p1));

  // One completeness point for name, no points for phone number (missing) or
  // email (not requested).
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  EXPECT_EQ(1, comp.GetContactCompletenessScore(&p2));

  // No completeness points, as the only field present was not requested.
  AutofillProfile p3 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "");
  EXPECT_EQ(0, comp.GetContactCompletenessScore(&p3));

  // Null profile returns 0.
  EXPECT_EQ(0, comp.GetContactCompletenessScore(nullptr));
}

TEST(PaymentRequestProfileUtilTest, IsContactInfoComplete) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerEmail);
  PaymentsProfileComparator comp("en-US", provider);

  // If name and email are present, return true regardless of the (ignored)
  // phone value.
  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "5551234567");
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");

  EXPECT_TRUE(comp.IsContactInfoComplete(&p1));
  EXPECT_TRUE(comp.IsContactInfoComplete(&p2));

  // If name is not present, return false regardless of the (ignored)
  // phone value.
  AutofillProfile p3 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "5551234567");
  AutofillProfile p4 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "");

  EXPECT_FALSE(comp.IsContactInfoComplete(&p3));
  EXPECT_FALSE(comp.IsContactInfoComplete(&p4));

  // If no fields are requested, any profile (even empty or null) is complete.
  MockPaymentOptionsProvider empty_provider(0);
  PaymentsProfileComparator empty_comp("en-US", empty_provider);

  AutofillProfile p5 = CreateProfileWithContactInfo("", "", "");

  EXPECT_TRUE(empty_comp.IsContactInfoComplete(&p1));
  EXPECT_TRUE(empty_comp.IsContactInfoComplete(&p5));
  EXPECT_TRUE(empty_comp.IsContactInfoComplete(nullptr));
}

}  // namespace profile_util
}  // namespace payments
