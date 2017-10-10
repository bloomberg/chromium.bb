// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#import "ios/chrome/browser/chrome_coordinator.h"
#import "ios/chrome/browser/ui/payments/address_edit_coordinator.h"
#import "ios/chrome/browser/ui/payments/contact_info_edit_coordinator.h"
#import "ios/chrome/browser/ui/payments/contact_info_selection_coordinator.h"
#import "ios/chrome/browser/ui/payments/credit_card_edit_coordinator.h"
#import "ios/chrome/browser/ui/payments/payment_items_display_coordinator.h"
#import "ios/chrome/browser/ui/payments/payment_method_selection_coordinator.h"
#include "ios/chrome/browser/ui/payments/payment_request_error_coordinator.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/browser/ui/payments/shipping_address_selection_coordinator.h"
#import "ios/chrome/browser/ui/payments/shipping_option_selection_coordinator.h"

namespace autofill {
class AutofillManager;
}  // namespace autofill

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace payments {
class PaymentDetails;
class PaymentRequest;
class PaymentShippingOption;
}  // namespace payments

@class PaymentRequestCoordinator;

// Delegate protocol for PaymentRequestCoordinator.
@protocol PaymentRequestCoordinatorDelegate<NSObject>

// Notifies the delegate that the user has confirmed the payment request.
- (void)paymentRequestCoordinatorDidConfirm:
    (PaymentRequestCoordinator*)coordinator;

// Notifies the delegate that the user has canceled the payment request.
- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator;

// Notifies the delegate that the user has selected to go to the card and
// address options page in Settings.
- (void)paymentRequestCoordinatorDidSelectSettings:
    (PaymentRequestCoordinator*)coordinator;

// Notifies the delegate that the user has selected a shipping address.
- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didSelectShippingAddress:
             (const autofill::AutofillProfile&)shippingAddress;

// Notifies the delegate that the user has selected a shipping option.
- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
          didSelectShippingOption:
              (const payments::PaymentShippingOption&)shippingOption;

// Notifies the delegate that the presenting view controller is dismissed.
- (void)paymentRequestCoordinatorDidStop:
    (PaymentRequestCoordinator*)coordinator;

@end

// Coordinator responsible for creating and presenting the PaymentRequest view
// controller. The PR view controller will be presented by the view controller
// provided in the initializer.
@interface PaymentRequestCoordinator
    : ChromeCoordinator<AddressEditCoordinatorDelegate,
                        ContactInfoEditCoordinatorDelegate,
                        ContactInfoSelectionCoordinatorDelegate,
                        CreditCardEditCoordinatorDelegate,
                        PaymentItemsDisplayCoordinatorDelegate,
                        PaymentMethodSelectionCoordinatorDelegate,
                        PaymentRequestErrorCoordinatorDelegate,
                        PaymentRequestViewControllerDelegate,
                        ShippingAddressSelectionCoordinatorDelegate,
                        ShippingOptionSelectionCoordinatorDelegate>

// The PaymentRequest object having a copy of payments::WebPaymentRequest as
// provided by the page invoking the Payment Request API. This pointer is not
// owned by this class and should outlive it.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// An instance of autofill::AutofillManager used for credit card unmasking. This
// reference is not owned by this class.
@property(nonatomic, assign) autofill::AutofillManager* autofillManager;

// An ios::ChromeBrowserState instance. This reference is not owned by this
// class.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The favicon of the page invoking the PaymentRequest API. Should be set before
// calling |start|.
@property(nonatomic, strong) UIImage* pageFavicon;

// The title of the page invoking the Payment Request API. Should be set before
// calling |start|.
@property(nonatomic, copy) NSString* pageTitle;

// The host of the page invoking the Payment Request API. Should be set before
// calling |start|.
@property(nonatomic, copy) NSString* pageHost;

// Whether or not the connection is secure.
@property(nonatomic, assign, getter=isConnectionSecure) BOOL connectionSecure;

// Whether or not the PaymentRequest view controller is in a pending state.
@property(nonatomic, assign, getter=isPending) BOOL pending;

// Whether or not the user can cancel out of the Payment Request view
// controller.
@property(nonatomic, assign, getter=isCancellable) BOOL cancellable;

// The delegate to be notified when the user confirms or cancels the request.
@property(nonatomic, weak) id<PaymentRequestCoordinatorDelegate> delegate;

// Initiates the UI that will request card details from the user.
- (void)
requestFullCreditCard:(const autofill::CreditCard&)card
       resultDelegate:
           (base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>)
               resultDelegate;

// Updates the payment details of the PaymentRequest and updates the UI.
- (void)updatePaymentDetails:(payments::PaymentDetails)paymentDetails;

// Displays an error message. Invokes |callback| when the message is dismissed.
- (void)displayErrorWithCallback:(ProceduralBlock)callback;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_COORDINATOR_H_
