// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <vector>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

@class EditorField;
@class PaymentRequestEditViewController;

// Delegate protocol for PaymentRequestEditViewController.
@protocol PaymentRequestEditViewControllerDelegate<NSObject>

// Notifies the delegate that the user has finished editing the fields supplied
// to the initializer. The value property of each field reflects the submitted
// value.
- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                  didFinishEditingFields:(NSArray<EditorField*>*)fields;

// Notifies the delegate that the user has chosen to return to the previous
// screen.
- (void)paymentRequestEditViewControllerDidReturn:
    (PaymentRequestEditViewController*)controller;

@end

// Validator protocol for PaymentRequestEditViewController.
@protocol PaymentRequestEditViewControllerValidator<NSObject>

// Returns the validation error string for |value|. |autofillType| corresponds
// to autofill::ServerFieldType. |required| indicates whether this is a required
// field. If there are no validation errors, an empty string is returned.
- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateValue:(NSString*)value
                                 autofillType:(NSInteger)autofillType
                                     required:(BOOL)required;

@end

// The collection view controller for a generic Payment Request edit screen. It
// features sections for every EditorField supplied to the initializer. Each
// section has a text field as well as an error message item which is visible
// when the value of its respective text field is invalid.
@interface PaymentRequestEditViewController : CollectionViewController

// The delegate to be notified when the user returns or finishes editing the
// fields.
@property(nonatomic, weak) id<PaymentRequestEditViewControllerDelegate>
    editorDelegate;

// The delegate to be called for validating the fields. By default, the
// controller is the validator.
@property(nonatomic, weak) id<PaymentRequestEditViewControllerValidator>
    validatorDelegate;

// Initializes this instance with a list of field definitions for the editor.
- (instancetype)initWithEditorFields:(NSArray<EditorField*>*)fields
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_H_
