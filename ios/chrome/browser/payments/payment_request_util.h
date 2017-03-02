// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/strings/string16.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace web {
class BasicCardResponse;
class PaymentAddress;
}  // namespace web

class PaymentRequest;

namespace payment_request_util {

// Helper function to get the name label from an autofill profile. Returns nil
// if the name field is empty.
NSString* GetNameLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to get the address label from an autofill profile. Returns
// nil if the address field is empty.
NSString* GetAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to get the phone number label from an autofill profile.
// Returns nil if the phone number field is empty.
NSString* GetPhoneNumberLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to get the email label from an autofill profile. Returns nil
// if the email field is empty.
NSString* GetEmailLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to get an instance of web::PaymentAddress from an autofill
// profile.
web::PaymentAddress GetPaymentAddressFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to get an instance of web::BasicCardResponse from an autofill
// credit card.
web::BasicCardResponse GetBasicCardResponseFromAutofillCreditCard(
    const PaymentRequest& payment_request,
    const autofill::CreditCard& card,
    const base::string16& cvc);

// Returns the title for the shipping section of the payment summary view given
// the shipping type specified in |payment_request|.
NSString* GetShippingSectionTitle(const PaymentRequest& payment_request);

// Returns the title for the shipping address selection view given the shipping
// type specified in |payment_request|.
NSString* GetShippingAddressSelectorTitle(
    const PaymentRequest& payment_request);

// Returns the informational message to be displayed in the shipping address
// selection view given the shipping type specified in |payment_request|.
NSString* GetShippingAddressSelectorInfoMessage(
    const PaymentRequest& payment_request);

// Returns the error message to be displayed in the shipping address selection
// view given the shipping type specified in |payment_request|.
NSString* GetShippingAddressSelectorErrorMessage(
    const PaymentRequest& payment_request);

// Returns the title for the shipping option selection view given the shipping
// type specified in |payment_request|.
NSString* GetShippingOptionSelectorTitle(const PaymentRequest& payment_request);

// Returns the error message to be displayed in the shipping option selection
// view given the shipping type specified in |payment_request|.
NSString* GetShippingOptionSelectorErrorMessage(
    const PaymentRequest& payment_request);

}  // namespace payment_request_util

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_
