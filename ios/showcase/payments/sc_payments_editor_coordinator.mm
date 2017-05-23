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
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCPaymentsEditorCoordinator ()<
    PaymentRequestEditViewControllerDataSource,
    PaymentRequestEditViewControllerValidator>

@property(nonatomic, strong)
    PaymentRequestEditViewController* paymentRequestEditViewController;

@property(nonatomic, strong) ProtocolAlerter* alerter;

@end

@implementation SCPaymentsEditorCoordinator

@synthesize state = _state;
@synthesize baseViewController = _baseViewController;
@synthesize paymentRequestEditViewController =
    _paymentRequestEditViewController;
@synthesize alerter = _alerter;

- (void)start {
  self.alerter = [[ProtocolAlerter alloc] initWithProtocols:@[
    @protocol(PaymentRequestEditViewControllerDelegate)
  ]];
  self.alerter.baseViewController = self.baseViewController;

  self.paymentRequestEditViewController =
      [[PaymentRequestEditViewController alloc]
          initWithStyle:CollectionViewControllerStyleAppBar];
  [self.paymentRequestEditViewController setTitle:@"Add info"];
  [self.paymentRequestEditViewController setEditorFields:[self editorFields]];
  [self.paymentRequestEditViewController setDataSource:self];
  [self.paymentRequestEditViewController
      setDelegate:static_cast<id<PaymentRequestEditViewControllerDelegate>>(
                      self.alerter)];
  [self.paymentRequestEditViewController setValidatorDelegate:self];
  [self.paymentRequestEditViewController loadModel];
  [self.baseViewController
      pushViewController:self.paymentRequestEditViewController
                animated:YES];
}

#pragma mark - Helper methods

- (NSArray<EditorField*>*)editorFields {
  EditorField* name =
      [[EditorField alloc] initWithAutofillUIType:AutofillUITypeProfileFullName
                                        fieldType:EditorFieldTypeTextField
                                            label:@"Name"
                                            value:@"John Doe"
                                         required:YES];
  EditorField* country = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomeAddressCountry
                   fieldType:EditorFieldTypeSelector
                       label:@"Country"
                       value:@"CAN"
                    required:YES];
  [country setDisplayValue:@"Canada"];
  EditorField* address = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomeAddressStreet
                   fieldType:EditorFieldTypeTextField
                       label:@"Address"
                       value:@""
                    required:YES];
  EditorField* postalCode = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomeAddressZip
                   fieldType:EditorFieldTypeTextField
                       label:@"Postal Code"
                       value:@""
                    required:NO];

  return @[ name, country, address, postalCode ];
}

#pragma mark - PaymentRequestEditViewControllerDataSource

- (CollectionViewItem*)headerItem {
  return nil;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  if (field.value.length)
    return nil;
  else if (field.isRequired)
    return @"Field is required";
  else
    return nil;
}

@end
