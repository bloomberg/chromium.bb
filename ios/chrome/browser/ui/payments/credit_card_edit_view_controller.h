// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/payments/credit_card_edit_view_controller_data_source.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"

@class CreditCardEditViewController;

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

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_CREDIT_CARD_EDIT_VIEW_CONTROLLER_H_
