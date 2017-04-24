// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/payments/sc_payments_editor_coordinator.h"

#import "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCPaymentsEditorCoordinator ()<
    PaymentRequestEditViewControllerDataSource> {
  PaymentRequestEditViewController* _paymentRequestEditViewController;
}
@end

@implementation SCPaymentsEditorCoordinator

@synthesize baseViewController = _baseViewController;

- (void)start {
  _paymentRequestEditViewController = [[PaymentRequestEditViewController alloc]
      initWithStyle:CollectionViewControllerStyleAppBar];
  [_paymentRequestEditViewController setTitle:@"Add info"];
  [_paymentRequestEditViewController setDataSource:self];
  [_paymentRequestEditViewController loadModel];
  [self.baseViewController pushViewController:_paymentRequestEditViewController
                                     animated:YES];
}


#pragma mark - PaymentRequestEditViewControllerDataSource

- (NSArray<EditorField*>*)editorFields {
  return @[
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeUnknown
                                          label:@"Name"
                                          value:@"John Doe"
                                       required:YES],
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeUnknown
                                          label:@"Address"
                                          value:@""
                                       required:YES],
    [[EditorField alloc] initWithAutofillUIType:AutofillUITypeUnknown
                                          label:@"Postal Code"
                                          value:@""
                                       required:NO]
  ];
}

@end
