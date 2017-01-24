// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTILS_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTILS_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payment_request_utils {

// Helper function to get the name label from an autofill profile.
NSString* NameLabelFromAutofillProfile(autofill::AutofillProfile* profile);

// Helper function to get the address label from an autofill profile.
NSString* AddressLabelFromAutofillProfile(autofill::AutofillProfile* profile);

// Helper function that returns a formatted currency string given the value and
// the currency code.
NSString* FormattedCurrencyString(NSDecimalNumber* value,
                                  NSString* currencyCode);

// Helper function to get the phone number label from an autofill profile.
NSString* PhoneNumberLabelFromAutofillProfile(
    autofill::AutofillProfile* profile);

// Helper function to get an instance of web::PaymentAddress from an autofill
// profile.
web::PaymentAddress PaymentAddressFromAutofillProfile(
    autofill::AutofillProfile* profile);

}  // namespace payment_request_utils

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTILS_H_
