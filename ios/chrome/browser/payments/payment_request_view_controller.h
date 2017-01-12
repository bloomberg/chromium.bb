// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}

@protocol PaymentRequestViewControllerDelegate<NSObject>
- (void)paymentRequestViewControllerDidCancel;
- (void)paymentRequestViewControllerDidConfirm;
- (void)paymentRequestViewControllerDisplayPaymentItems;
- (void)paymentRequestViewControllerSelectShippingAddress;
- (void)paymentRequestViewControllerSelectShippingOption;
- (void)paymentRequestViewControllerSelectPaymentMethod;

@end

// View controller responsible for presenting the details of a PaymentRequest to
// the user and communicating their choices to the supplied delegate.
@interface PaymentRequestViewController : CollectionViewController

// The PaymentRequest object as provided by the page invoking the Payment
// Request API.
@property(nonatomic, assign) web::PaymentRequest paymentRequest;

// The favicon of the page invoking the Payment Request API.
@property(nonatomic, retain) UIImage* pageFavicon;

// The title of the page invoking the Payment Request API.
@property(nonatomic, copy) NSString* pageTitle;

// The host of the page invoking the Payment Request API.
@property(nonatomic, copy) NSString* pageHost;

// The currently selected and displayed shipping address, if any.
@property(nonatomic, assign) autofill::AutofillProfile* selectedShippingAddress;

// The currently selected and displayed shipping option, if any.
@property(nonatomic, assign) web::PaymentShippingOption* selectedShippingOption;

// The currently selected and displayed payment method, if any.
@property(nonatomic, assign) autofill::CreditCard* selectedPaymentMethod;

// The delegate to be notified when the user confirms or cancels the request.
@property(nonatomic, weak) id<PaymentRequestViewControllerDelegate> delegate;

// Sets the selected shipping address and updates the UI.
- (void)updateSelectedShippingAddress:
    (autofill::AutofillProfile*)shippingAddress;

// Sets the selected shipping option and updates the UI.
- (void)updateSelectedShippingOption:
    (web::PaymentShippingOption*)shippingOption;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_
