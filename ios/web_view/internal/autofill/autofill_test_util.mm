// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/autofill_test_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/country_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {
namespace testing {
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
NSString* const kCardHolderFullName = @"Homer Simpson";
NSString* const kCardNumber = @"4408041234567893";
NSString* const kNetworkName = @"Visa";
NSString* const kExpirationMonth = @"08";
NSString* const kExpirationYear = @"2038";

autofill::AutofillProfile CreateTestProfile() {
  autofill::CountryNames::SetLocaleString(base::SysNSStringToUTF8(kLocale));
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
  return profile;
}

autofill::CreditCard CreateTestCreditCard() {
  autofill::CreditCard credit_card(base::SysNSStringToUTF8(kGuid),
                                   base::SysNSStringToUTF8(kOrigin));
  credit_card.SetInfo(autofill::CREDIT_CARD_NAME_FULL,
                      base::SysNSStringToUTF16(kCardHolderFullName),
                      base::SysNSStringToUTF8(kLocale));
  credit_card.SetInfo(autofill::CREDIT_CARD_NUMBER,
                      base::SysNSStringToUTF16(kCardNumber),
                      base::SysNSStringToUTF8(kLocale));
  credit_card.SetInfo(autofill::CREDIT_CARD_EXP_MONTH,
                      base::SysNSStringToUTF16(kExpirationMonth),
                      base::SysNSStringToUTF8(kLocale));
  credit_card.SetInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                      base::SysNSStringToUTF16(kExpirationYear),
                      base::SysNSStringToUTF8(kLocale));
  return credit_card;
}
}  // namespace testing
}  // namespace ios_web_view
