// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

class PaymentRequest;

namespace payment_request_util {

// Helper function to get the name label from an autofill profile. Returns nil
// if the name field is empty.
NSString* GetNameLabelFromAutofillProfile(autofill::AutofillProfile* profile);

// Helper function to get the address label from an autofill profile. Returns
// nil if the address field is empty.
NSString* GetAddressLabelFromAutofillProfile(
    autofill::AutofillProfile* profile);

// Helper function to get the phone number label from an autofill profile.
// Returns nil if the phone number field is empty.
NSString* GetPhoneNumberLabelFromAutofillProfile(
    autofill::AutofillProfile* profile);

// Helper function to get the email label from an autofill profile. Returns nil
// if the email field is empty.
NSString* GetEmailLabelFromAutofillProfile(autofill::AutofillProfile* profile);

// Helper function to get an instance of web::PaymentAddress from an autofill
// profile.
web::PaymentAddress GetPaymentAddressFromAutofillProfile(
    autofill::AutofillProfile* profile);

// Returns the title for the shipping section of the payment summary view given
// the shipping type specified in |payment_request|.
NSString* GetShippingSectionTitle(PaymentRequest* payment_request);

// Returns the title for the shipping address selection view given the shipping
// type specified in |payment_request|.
NSString* GetShippingAddressSelectorTitle(PaymentRequest* payment_request);

// Returns the informational message to be displayed in the shipping address
// selection view given the shipping type specified in |payment_request|.
NSString* GetShippingAddressSelectorInfoMessage(PaymentRequest* paymentRequest);

// Returns the error message to be displayed in the shipping address selection
// view given the shipping type specified in |payment_request|.
NSString* GetShippingAddressSelectorErrorMessage(
    PaymentRequest* paymentRequest);

// Returns the title for the shipping option selection view given the shipping
// type specified in |payment_request|.
NSString* GetShippingOptionSelectorTitle(PaymentRequest* payment_request);

// Returns the error message to be displayed in the shipping option selection
// view given the shipping type specified in |payment_request|.
NSString* GetShippingOptionSelectorErrorMessage(PaymentRequest* paymentRequest);

}  // namespace payment_request_util

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_
