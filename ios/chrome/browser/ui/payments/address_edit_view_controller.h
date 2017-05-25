// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_ADDRESS_EDIT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_ADDRESS_EDIT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"

@class AddressEditViewController;
@class EditorField;

// Delegate protocol for AddressEditViewController.
@protocol
    AddressEditViewControllerDelegate<PaymentRequestEditViewControllerDelegate>

// Notifies the delegate that the user has finished editing the address editor
// fields.
- (void)addressEditViewController:(AddressEditViewController*)controller
           didFinishEditingFields:(NSArray<EditorField*>*)fields;

// Notifies the delegate that the user has chosen to discard entries in the
// address editor fields and return to the previous screen.
- (void)addressEditViewControllerDidCancel:
    (AddressEditViewController*)controller;

@end

// View controller responsible for presenting an address edit form. The form
// features text fields for the field definitions passed to the initializer.
@interface AddressEditViewController : PaymentRequestEditViewController

// The delegate to be notified when the user returns or finishes editing the
// address editor fields.
@property(nonatomic, weak) id<AddressEditViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_ADDRESS_EDIT_VIEW_CONTROLLER_H_
