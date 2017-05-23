// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_ADDRESS_EDIT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_ADDRESS_EDIT_MEDIATOR_H_

#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller_data_source.h"
#import "ios/chrome/browser/ui/payments/region_data_loader.h"

class PaymentRequest;
@protocol PaymentRequestEditConsumer;

namespace autofill {
class AutofillProfile;
}  // namespace autofill

// Serves as data source for AddressEditViewController.
@interface AddressEditMediator
    : NSObject<PaymentRequestEditViewControllerDataSource,
               RegionDataLoaderConsumer>

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, weak) id<PaymentRequestEditConsumer> consumer;

// The map of country codes to country names.
@property(nonatomic, strong) NSDictionary<NSString*, NSString*>* countries;

// The country code for the currently selected country, if any.
@property(nonatomic, strong) NSString* selectedCountryCode;

// The list of region names used for the autofill::ADDRESS_HOME_STATE field.
@property(nonatomic, strong) NSArray<NSString*>* regions;

// Initializes this object with an instance of PaymentRequest which has a copy
// of web::PaymentRequest as provided by the page invoking the Payment Request
// API as well as |address| which is the address to be edited, if any.
// This object will not take ownership of |paymentRequest| or |address|.
- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
                               address:(autofill::AutofillProfile*)address
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_ADDRESS_EDIT_MEDIATOR_H_
