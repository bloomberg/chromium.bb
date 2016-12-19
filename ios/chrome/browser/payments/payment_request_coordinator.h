// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/chrome_coordinator.h"
#import "ios/chrome/browser/payments/payment_items_display_coordinator.h"
#import "ios/chrome/browser/payments/payment_method_selection_coordinator.h"
#import "ios/chrome/browser/payments/payment_request_view_controller.h"
#import "ios/chrome/browser/payments/shipping_address_selection_coordinator.h"
#include "ios/web/public/payments/payment_request.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
class PersonalDataManager;
}

@protocol PaymentRequestCoordinatorDelegate<NSObject>
- (void)paymentRequestCoordinatorDidCancel;
- (void)paymentRequestCoordinatorDidConfirm:
    (web::PaymentResponse)paymentResponse;

@end

// Coordinator responsible for creating and presenting the PaymentRequest view
// controller. The PR view controller will be presented by the view controller
// provided in the initializer.
@interface PaymentRequestCoordinator
    : ChromeCoordinator<PaymentRequestViewControllerDelegate,
                        PaymentItemsDisplayCoordinatorDelegate,
                        PaymentMethodSelectionCoordinatorDelegate,
                        ShippingAddressSelectionCoordinatorDelegate>

// Creates a Payment Request coordinator that will present UI on
// |viewController| using data available from |personalDataManager|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                       personalDataManager:
                           (autofill::PersonalDataManager*)personalDataManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;

// The PaymentRequest object as provided by the page invoking the Payment
// Request API. Should be set before calling |start|.
@property(nonatomic, assign) web::PaymentRequest paymentRequest;

// The favicon of the page invoking the PaymentRequest API. Should be set before
// calling |start|.
@property(nonatomic, retain) UIImage* pageFavicon;

// The title of the page invoking the Payment Request API. Should be set before
// calling |start|.
@property(nonatomic, copy) NSString* pageTitle;

// The host of the page invoking the Payment Request API. Should be set before
// calling |start|.
@property(nonatomic, copy) NSString* pageHost;

// The currently selected shipping address, if any.
@property(nonatomic, readonly)
    autofill::AutofillProfile* selectedShippingAddress;

// The payment method selected by the user, if any.
@property(nonatomic, readonly) autofill::CreditCard* selectedPaymentMethod;

// The delegate to be notified when the user confirms or cancels the request.
@property(nonatomic, weak) id<PaymentRequestCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_COORDINATOR_H_
