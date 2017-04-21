// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/payments/payment_request_edit_view_controller.h"

@class EditorField;
@class CreditCardEditViewController;

typedef NS_ENUM(NSInteger, CreditCardEditViewControllerState) {
  // The view controller is used to create a new credit card.
  CreditCardEditViewControllerStateCreate,
  // The view controller is used to edit a credit card.
  CreditCardEditViewControllerStateEdit,
};

// Data source protocol for CreditCardEditViewController.
@protocol CreditCardEditViewControllerDataSource<
    PaymentRequestEditViewControllerDataSource>

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

// Delegate protocol for CreditCardEditViewController.
@protocol CreditCardEditViewControllerDelegate<NSObject>

// Notifies the delegate that the user has finished editing the credit card
// editor fields.
- (void)creditCardEditViewController:(CreditCardEditViewController*)controller
              didFinishEditingFields:(NSArray<EditorField*>*)fields
                    billingAddressID:(NSString*)billingAddressID
                      saveCreditCard:(BOOL)saveCard;

// Notifies the delegate that the user has chosen to discard entries in the
// credit card editor fields and return to the previous screen.
- (void)creditCardEditViewControllerDidCancel:
    (CreditCardEditViewController*)controller;

@end

// View controller responsible for presenting a credit card edit form. The form
// features text fields for the field definitions passed to the initializer in
// addition to an item displaying the billing address associated with the credit
// card, if any.
@interface CreditCardEditViewController : PaymentRequestEditViewController

// The delegate to be notified when the user returns or finishes editing the
// credit card editor fields.
@property(nonatomic, weak) id<CreditCardEditViewControllerDelegate> delegate;

// The data source for this view controller.
@property(nonatomic, weak) id<CreditCardEditViewControllerDataSource>
    dataSource;

// The billing address GUID of the card being editted, if any.
@property(nonatomic, copy) NSString* billingAddressGUID;

// The view controller's state. i.e., whether a new credit card is being created
// or one is being edited.
@property(nonatomic, assign) CreditCardEditViewControllerState state;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_H_
