// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_

#import <Foundation/Foundation.h>

#include <vector>

#include "base/strings/string16.h"
#include "components/payments/core/payment_options_provider.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

class PaymentRequest;

namespace payment_request_util {

// Helper function to create a name label from an autofill profile. Returns nil
// if the resulting label is empty.
NSString* GetNameLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to create a shipping address label from an autofill profile.
// Returns nil if the resulting label is empty.
NSString* GetShippingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to create a billing address label from an autofill profile.
// Returns nil if the resulting label is empty.
NSString* GetBillingAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to create a phone number label from an autofill profile.
// Returns nil if the resulting label is empty.
NSString* GetPhoneNumberLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to create an email label from an autofill profile. Returns
// nil if the resulting label is empty.
NSString* GetEmailLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile);

// Helper function to create a notification label for an address cell from an
// autofill profile. Returns nil if the resulting label is empty.
NSString* GetAddressNotificationLabelFromAutofillProfile(
    PaymentRequest& payment_request,
    const autofill::AutofillProfile& profile);

// Returns whether the credit card is complete to be used as a payment method
// without further editing.
BOOL IsCreditCardCompleteForPayment(
    const autofill::CreditCard& credit_card,
    const std::vector<autofill::AutofillProfile*>& billing_profiles);

// Helper function to create a notification label for what's missing from a
// credit card. Returns nil if the resulting label is empty.
NSString* GetPaymentMethodNotificationLabelFromCreditCard(
    const autofill::CreditCard& credit_card,
    const std::vector<autofill::AutofillProfile*>& billing_profiles);

// Returns the title for the shipping section of the payment summary view given
// the shipping type specified in |payment_request|.
NSString* GetShippingSectionTitle(payments::PaymentShippingType shipping_type);

// Returns the error message to be displayed in the shipping address selection
// view given the shipping type specified in |payment_request|.
NSString* GetShippingAddressSelectorErrorMessage(
    const PaymentRequest& payment_request);

// Returns the error message to be displayed in the shipping option selection
// view given the shipping type specified in |payment_request|.
NSString* GetShippingOptionSelectorErrorMessage(
    const PaymentRequest& payment_request);

}  // namespace payment_request_util

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTIL_H_
