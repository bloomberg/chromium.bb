// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_AUTOFILL_TEST_UTIL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_AUTOFILL_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"

namespace ios_web_view {
namespace testing {
// Shared attributes for test profile and test credit card.
extern NSString* const kLocale;
extern NSString* const kGuid;
extern NSString* const kOrigin;
// Test profile attributes.
extern NSString* const kName;
extern NSString* const kCompany;
extern NSString* const kAddress1;
extern NSString* const kAddress2;
extern NSString* const kCity;
extern NSString* const kState;
extern NSString* const kZipcode;
extern NSString* const kCountry;
extern NSString* const kPhone;
extern NSString* const kEmail;
// Test credit card attributes.
extern NSString* const kCardHolderFullName;
extern NSString* const kCardNumber;
extern NSString* const kNetworkName;
extern NSString* const kExpirationMonth;
extern NSString* const kExpirationYear;

// Creates and returns an autofill profile for testing.
autofill::AutofillProfile CreateTestProfile();

// Creates and returns a credit card for testing.
autofill::CreditCard CreateTestCreditCard();
}  // namespace testing
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_AUTOFILL_TEST_UTIL_H_
