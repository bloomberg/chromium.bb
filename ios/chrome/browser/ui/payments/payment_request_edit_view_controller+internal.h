// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_INTERNAL_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_INTERNAL_H_

#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller_actions.h"

// Use these as the starting values for section identifier and item type enums
// in subclasses. These values are chosen to prevent overlapping with the
// section identifier and item type enums of this class.
const NSInteger kSectionIdentifierEnumStart = kSectionIdentifierEnumZero + 20;
const NSInteger kItemTypeEnumStart = kItemTypeEnumZero + 100;

// Internal API for subclasses of PaymentRequestEditViewController.
@interface PaymentRequestEditViewController (
    Internal)<PaymentRequestEditViewControllerActions>

// Validates each field. If there is a validation error, displays an error
// message item in the same section as the field and returns NO. Otherwise
// removes the error message item in that section if one exists and sets the
// value on the field. Returns YES if all the fields are validated successfully.
- (BOOL)validateForm;

// Called after the editor field items have been added to the the collection
// view model. Subclasses override this method to add items after the editor
// fields.
- (void)loadFooterItems;

// Returns the index path for the cell associated with the currently focused
// text field.
- (NSIndexPath*)indexPathForCurrentTextField;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_EDIT_VIEW_CONTROLLER_INTERNAL_H_
