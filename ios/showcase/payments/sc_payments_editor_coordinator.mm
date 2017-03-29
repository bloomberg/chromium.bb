// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/payments/sc_payments_editor_coordinator.h"

#import "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/payments/payment_request_editor_field.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/settings/cells/autofill_edit_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCPaymentsEditorCoordinator ()<
    PaymentRequestEditViewControllerDelegate> {
  PaymentRequestEditViewController* _paymentRequestEditViewController;
}
@end

@implementation SCPaymentsEditorCoordinator

@synthesize baseViewController = _baseViewController;

- (void)start {
  NSArray<EditorField*>* editorFields = [self editorFields];
  _paymentRequestEditViewController = [[PaymentRequestEditViewController alloc]
      initWithEditorFields:editorFields];
  [_paymentRequestEditViewController setTitle:@"Add info"];
  [_paymentRequestEditViewController setEditorDelegate:self];
  [_paymentRequestEditViewController loadModel];
  [self.baseViewController pushViewController:_paymentRequestEditViewController
                                     animated:YES];
}

#pragma mark - PaymentRequestEditViewControllerDelegate

- (void)paymentRequestEditViewController:
            (PaymentRequestEditViewController*)controller
                  didFinishEditingFields:(NSArray<EditorField*>*)fields {
  [self.baseViewController popViewControllerAnimated:YES];
}

- (void)paymentRequestEditViewControllerDidReturn:
    (PaymentRequestEditViewController*)controller {
  [self.baseViewController popViewControllerAnimated:YES];
}

#pragma mark - Helper methods

- (NSArray<EditorField*>*)editorFields {
  return @[
    [[EditorField alloc]
        initWithAutofillType:static_cast<NSInteger>(autofill::UNKNOWN_TYPE)
                       label:@"Name"
                       value:@"John Doe"
                    required:YES],
    [[EditorField alloc]
        initWithAutofillType:static_cast<NSInteger>(autofill::UNKNOWN_TYPE)
                       label:@"Address"
                       value:@""
                    required:YES],
    [[EditorField alloc]
        initWithAutofillType:static_cast<NSInteger>(autofill::UNKNOWN_TYPE)
                       label:@"Postal Code"
                       value:@""
                    required:NO]
  ];
}

@end
