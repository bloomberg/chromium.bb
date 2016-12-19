// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTILS_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTILS_H_

#import <Foundation/Foundation.h>

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payment_request_utils {

// Helper function to get the shipping address label from an autofill profile.
NSString* AddressLabelFromAutofillProfile(autofill::AutofillProfile* profile);

}  // namespace payment_request_utils

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_UTILS_H_
