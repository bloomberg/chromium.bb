// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <vector>

#import "ios/chrome/browser/payments/payment_request_edit_view_controller_data_source.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

extern NSString* const kWarningMessageAccessibilityID;

@class PaymentRequestEditViewController;

// Validator protocol for PaymentRequestEditViewController.
@protocol PaymentRequestEditViewControllerValidator<NSObject>

// Returns the validation error string for |value| which has the type
// |autofillUIType|. |required| indicates whether this is a required field.
// Returns nil if there are no validation errors.
- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateValue:(NSString*)value
                               autofillUIType:(AutofillUIType)autofillUIType
                                     required:(BOOL)required;

@end

// The collection view controller for a generic Payment Request edit screen. It
// features sections for every EditorField supplied to the initializer. Each
// section has a text field as well as an error message item which is visible
// when the value of its respective text field is invalid.
@interface PaymentRequestEditViewController : CollectionViewController

// The data source for this view controller.
@property(nonatomic, weak) id<PaymentRequestEditViewControllerDataSource>
    dataSource;

// The delegate to be called for validating the fields. By default, the
// controller is the validator.
@property(nonatomic, weak) id<PaymentRequestEditViewControllerValidator>
    validatorDelegate;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_H_
