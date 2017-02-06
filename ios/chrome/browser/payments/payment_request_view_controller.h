// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
}

extern NSString* const kPaymentRequestCollectionViewId;

@class PaymentRequestViewController;

// Delegate protocol for PaymentRequestViewController.
@protocol PaymentRequestViewControllerDelegate<NSObject>

// Notifies the delegate that the user has canceled the payment request.
- (void)paymentRequestViewControllerDidCancel:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has confirmed the payment request.
- (void)paymentRequestViewControllerDidConfirm:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has selected the payment summary item.
- (void)paymentRequestViewControllerDidSelectPaymentSummaryItem:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has selected the shipping address item.
- (void)paymentRequestViewControllerDidSelectShippingAddressItem:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has selected the shipping option item.
- (void)paymentRequestViewControllerDidSelectShippingOptionItem:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has selected the payment method item.
- (void)paymentRequestViewControllerDidSelectPaymentMethodItem:
    (PaymentRequestViewController*)controller;

@end

// View controller responsible for presenting the details of a PaymentRequest to
// the user and communicating their choices to the supplied delegate.
@interface PaymentRequestViewController : CollectionViewController

// The favicon of the page invoking the Payment Request API.
@property(nonatomic, retain) UIImage* pageFavicon;

// The title of the page invoking the Payment Request API.
@property(nonatomic, copy) NSString* pageTitle;

// The host of the page invoking the Payment Request API.
@property(nonatomic, copy) NSString* pageHost;

// The delegate to be notified when the user confirms or cancels the request.
@property(nonatomic, weak) id<PaymentRequestViewControllerDelegate> delegate;

// Updates the payment summary section UI.
- (void)updatePaymentSummarySection;

// Sets the selected shipping address and updates the UI.
- (void)updateSelectedShippingAddress:
    (autofill::AutofillProfile*)shippingAddress;

// Sets the selected shipping option and updates the UI.
- (void)updateSelectedShippingOption:
    (web::PaymentShippingOption*)shippingOption;

// Initializes this object with an instance of PaymentRequest which owns an
// instance of web::PaymentRequest as provided by the page invoking the Payment
// Request API. This object will not take ownership of |paymentRequest|.
- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_
