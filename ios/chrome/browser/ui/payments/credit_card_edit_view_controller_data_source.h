// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller_data_source.h"

@class CollectionViewItem;

// The possible states the view controller can be in.
typedef NS_ENUM(NSInteger, CreditCardEditViewControllerState) {
  // The view controller is used to create a new credit card.
  CreditCardEditViewControllerStateCreate,
  // The view controller is used to edit a credit card.
  CreditCardEditViewControllerStateEdit,
};

// Data source protocol for CreditCardEditViewController.
@protocol CreditCardEditViewControllerDataSource<
    PaymentRequestEditViewControllerDataSource>

// The current state of the view controller.
@property(nonatomic, assign) CreditCardEditViewControllerState state;

// Returns an item that identifies the credit card being edited. May return nil.
- (CollectionViewItem*)serverCardSummaryItem;

// Returns an item that displays a list of payment method type icons for the
// accepted payment methods. May return nil.
- (CollectionViewItem*)acceptedPaymentMethodsItem;

// Returns the billing address label from an autofill profile with the given
// guid. Returns nil if the profile does not have an address.
- (NSString*)billingAddressLabelForProfileWithGUID:(NSString*)profileGUID;

// Returns the credit card type icon corresponding to |cardNumber|.
- (UIImage*)cardTypeIconFromCardNumber:(NSString*)cardNumber;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_DATA_SOURCE_H_
