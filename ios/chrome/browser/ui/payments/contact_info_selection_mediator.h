// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CONTACT_INFO_SELECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CONTACT_INFO_SELECTION_MEDIATOR_H_

#import "ios/chrome/browser/ui/payments/payment_request_selector_view_controller_data_source.h"

class PaymentRequest;

// Serves as data source for PaymentRequestSelectorViewController.
@interface ContactInfoSelectionMediator
    : NSObject<PaymentRequestSelectorViewControllerDataSource>

// Redefined to be read-write.
@property(nonatomic, readwrite, assign) NSUInteger selectedItemIndex;

// Initializes this object with an instance of PaymentRequest which has a copy
// of web::PaymentRequest as provided by the page invoking the Payment Request
// API. This object will not take ownership of |paymentRequest|.
- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CONTACT_INFO_SELECTION_MEDIATOR_H_
