// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"

#import <Foundation/Foundation.h>
#include <string>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {
NSString* const kLocale = @"en";
NSString* const kGuid = @"12345";
NSString* const kOrigin = @"www.chromium.org";
NSString* const kName = @"Homer Simpson";
NSString* const kCompany = @"Chromium";
NSString* const kAddress1 = @"123 Main Street";
NSString* const kAddress2 = @"Apt 1337";
NSString* const kCity = @"Springfield";
NSString* const kState = @"Illinois";
NSString* const kZipcode = @"55123";
NSString* const kCountry = @"United States";
NSString* const kPhone = @"3103106000";
NSString* const kEmail = @"hjs@aol.com";
}

class CWVAutofillProfileTest : public PlatformTest {
 protected:
  CWVAutofillProfileTest() {
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        base::SysNSStringToUTF8(kLocale), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  ~CWVAutofillProfileTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }
};

// Tests CWVAutofillProfile initialization.
TEST_F(CWVAutofillProfileTest, Initialization) {
  autofill::AutofillProfile profile(base::SysNSStringToUTF8(kGuid),
                                    base::SysNSStringToUTF8(kOrigin));
  profile.SetInfo(autofill::NAME_FULL, base::SysNSStringToUTF16(kName),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::COMPANY_NAME, base::SysNSStringToUTF16(kCompany),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::ADDRESS_HOME_LINE1,
                  base::SysNSStringToUTF16(kAddress1),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::ADDRESS_HOME_LINE2,
                  base::SysNSStringToUTF16(kAddress2),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::ADDRESS_HOME_CITY, base::SysNSStringToUTF16(kCity),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::ADDRESS_HOME_STATE,
                  base::SysNSStringToUTF16(kState),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::ADDRESS_HOME_ZIP,
                  base::SysNSStringToUTF16(kZipcode),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::ADDRESS_HOME_COUNTRY,
                  base::SysNSStringToUTF16(kCountry),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                  base::SysNSStringToUTF16(kPhone),
                  base::SysNSStringToUTF8(kLocale));
  profile.SetInfo(autofill::EMAIL_ADDRESS, base::SysNSStringToUTF16(kEmail),
                  base::SysNSStringToUTF8(kLocale));
  CWVAutofillProfile* cwv_profile =
      [[CWVAutofillProfile alloc] initWithProfile:profile];

  EXPECT_NSEQ(kName, cwv_profile.name);
  EXPECT_NSEQ(kCompany, cwv_profile.company);
  EXPECT_NSEQ(kAddress1, cwv_profile.address1);
  EXPECT_NSEQ(kAddress2, cwv_profile.address2);
  EXPECT_NSEQ(kCity, cwv_profile.city);
  EXPECT_NSEQ(kState, cwv_profile.state);
  EXPECT_NSEQ(kZipcode, cwv_profile.zipcode);
  EXPECT_NSEQ(kCountry, cwv_profile.country);
  EXPECT_NSEQ(kPhone, cwv_profile.phone);
  EXPECT_NSEQ(kEmail, cwv_profile.email);
}

// Tests CWVAutofillProfile updates properties.
TEST_F(CWVAutofillProfileTest, ModifyProperties) {
  autofill::AutofillProfile profile(base::SysNSStringToUTF8(kGuid),
                                    base::SysNSStringToUTF8(kOrigin));
  CWVAutofillProfile* cwv_profile =
      [[CWVAutofillProfile alloc] initWithProfile:profile];
  cwv_profile.name = kName;
  cwv_profile.company = kCompany;
  cwv_profile.address1 = kAddress1;
  cwv_profile.address2 = kAddress2;
  cwv_profile.city = kCity;
  cwv_profile.state = kState;
  cwv_profile.zipcode = kZipcode;
  cwv_profile.country = kCountry;
  cwv_profile.phone = kPhone;
  cwv_profile.email = kEmail;

  EXPECT_NSEQ(kName, cwv_profile.name);
  EXPECT_NSEQ(kCompany, cwv_profile.company);
  EXPECT_NSEQ(kAddress1, cwv_profile.address1);
  EXPECT_NSEQ(kAddress2, cwv_profile.address2);
  EXPECT_NSEQ(kCity, cwv_profile.city);
  EXPECT_NSEQ(kState, cwv_profile.state);
  EXPECT_NSEQ(kZipcode, cwv_profile.zipcode);
  EXPECT_NSEQ(kCountry, cwv_profile.country);
  EXPECT_NSEQ(kPhone, cwv_profile.phone);
  EXPECT_NSEQ(kEmail, cwv_profile.email);
}

}  // namespace ios_web_view
