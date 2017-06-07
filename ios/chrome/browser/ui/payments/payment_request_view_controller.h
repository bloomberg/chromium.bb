// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

extern NSString* const kPaymentRequestCollectionViewID;

class PaymentRequest;

@class PaymentRequestViewController;

// Data source protocol for PaymentRequestViewController.
@protocol PaymentRequestViewControllerDataSource<NSObject>

// Returns the authenticated account name, if a user is authenticated.
// Otherwise, returns nil.
- (NSString*)authenticatedAccountName;

@end

// Delegate protocol for PaymentRequestViewController.
@protocol PaymentRequestViewControllerDelegate<NSObject>

// Notifies the delegate that the user has canceled the payment request.
- (void)paymentRequestViewControllerDidCancel:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has confirmed the payment request.
- (void)paymentRequestViewControllerDidConfirm:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has selected to go to the card and
// address options page in Settings.
- (void)paymentRequestViewControllerDidSelectSettings:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has selected the payment summary item.
- (void)paymentRequestViewControllerDidSelectPaymentSummaryItem:
    (PaymentRequestViewController*)controller;

// Notifies the delegate that the user has selected the contact info item.
- (void)paymentRequestViewControllerDidSelectContactInfoItem:
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
@interface PaymentRequestViewController
    : CollectionViewController<CollectionViewFooterLinkDelegate>

// The favicon of the page invoking the Payment Request API.
@property(nonatomic, strong) UIImage* pageFavicon;

// The title of the page invoking the Payment Request API.
@property(nonatomic, copy) NSString* pageTitle;

// The host of the page invoking the Payment Request API.
@property(nonatomic, copy) NSString* pageHost;

// Whether or not the connection is secure.
@property(nonatomic, assign, getter=isConnectionSecure) BOOL connectionSecure;

// Whether or not the view is in a pending state.
@property(nonatomic, assign, getter=isPending) BOOL pending;

// The delegate to be notified when the user confirms or cancels the request.
@property(nonatomic, weak) id<PaymentRequestViewControllerDelegate> delegate;

// Whether the data source should be shown (usually until the first payment
// has been completed) or not.
@property(nonatomic, assign) BOOL showPaymentDataSource;

@property(nonatomic, weak) id<PaymentRequestViewControllerDataSource>
    dataSource;

// Updates the payment summary section UI. If |totalValueChanged| is YES,
// adds a label to the total amount item indicating that the total amount was
// updated.
- (void)updatePaymentSummaryWithTotalValueChanged:(BOOL)totalValueChanged;

// Updates the selected shipping address.
- (void)updateSelectedShippingAddressUI;

// Updates the selected shipping option.
- (void)updateSelectedShippingOptionUI;

// Updates the selected payment method.
- (void)updateSelectedPaymentMethodUI;

// Updates the selected contact info.
- (void)updateSelectedContactInfoUI;

// Initializes this object with an instance of PaymentRequest which has a copy
// of web::PaymentRequest as provided by the page invoking the Payment Request
// API. This object will not take ownership of |paymentRequest|.
- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_VIEW_CONTROLLER_H_
