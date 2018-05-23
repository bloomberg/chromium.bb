// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"

#import <Foundation/Foundation.h>
#include <string>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#import "ios/web_view/internal/autofill/autofill_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVAutofillProfileTest : public PlatformTest {
 protected:
  CWVAutofillProfileTest() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        base::SysNSStringToUTF8(testing::kLocale), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  ~CWVAutofillProfileTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }
};

// Tests CWVAutofillProfile initialization.
TEST_F(CWVAutofillProfileTest, Initialization) {
  CWVAutofillProfile* cwv_profile =
      [[CWVAutofillProfile alloc] initWithProfile:testing::CreateTestProfile()];

  EXPECT_NSEQ(testing::kName, cwv_profile.name);
  EXPECT_NSEQ(testing::kCompany, cwv_profile.company);
  EXPECT_NSEQ(testing::kAddress1, cwv_profile.address1);
  EXPECT_NSEQ(testing::kAddress2, cwv_profile.address2);
  EXPECT_NSEQ(testing::kCity, cwv_profile.city);
  EXPECT_NSEQ(testing::kState, cwv_profile.state);
  EXPECT_NSEQ(testing::kZipcode, cwv_profile.zipcode);
  EXPECT_NSEQ(testing::kCountry, cwv_profile.country);
  EXPECT_NSEQ(testing::kPhone, cwv_profile.phone);
  EXPECT_NSEQ(testing::kEmail, cwv_profile.email);
}

// Tests CWVAutofillProfile updates properties.
TEST_F(CWVAutofillProfileTest, ModifyProperties) {
  autofill::AutofillProfile profile(base::SysNSStringToUTF8(testing::kGuid),
                                    base::SysNSStringToUTF8(testing::kOrigin));
  CWVAutofillProfile* cwv_profile =
      [[CWVAutofillProfile alloc] initWithProfile:profile];
  cwv_profile.name = testing::kName;
  cwv_profile.company = testing::kCompany;
  cwv_profile.address1 = testing::kAddress1;
  cwv_profile.address2 = testing::kAddress2;
  cwv_profile.city = testing::kCity;
  cwv_profile.state = testing::kState;
  cwv_profile.zipcode = testing::kZipcode;
  cwv_profile.country = testing::kCountry;
  cwv_profile.phone = testing::kPhone;
  cwv_profile.email = testing::kEmail;

  EXPECT_NSEQ(testing::kName, cwv_profile.name);
  EXPECT_NSEQ(testing::kCompany, cwv_profile.company);
  EXPECT_NSEQ(testing::kAddress1, cwv_profile.address1);
  EXPECT_NSEQ(testing::kAddress2, cwv_profile.address2);
  EXPECT_NSEQ(testing::kCity, cwv_profile.city);
  EXPECT_NSEQ(testing::kState, cwv_profile.state);
  EXPECT_NSEQ(testing::kZipcode, cwv_profile.zipcode);
  EXPECT_NSEQ(testing::kCountry, cwv_profile.country);
  EXPECT_NSEQ(testing::kPhone, cwv_profile.phone);
  EXPECT_NSEQ(testing::kEmail, cwv_profile.email);
}

}  // namespace ios_web_view
