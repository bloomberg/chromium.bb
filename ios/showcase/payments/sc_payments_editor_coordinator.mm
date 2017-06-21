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
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#import "ios/showcase/common/protocol_alerter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCPaymentsEditorMediator
    : NSObject<PaymentRequestEditViewControllerDataSource,
               PaymentRequestEditViewControllerValidator>

// The reference to the province field.
@property(nonatomic, strong) EditorField* province;

// The consumer for this object.
@property(nonatomic, weak) id<PaymentRequestEditConsumer> consumer;

@end

@implementation SCPaymentsEditorMediator

@synthesize state = _state;
@synthesize province = _province;
@synthesize consumer = _consumer;

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setEditorFields:[self editorFields]];
}

- (void)loadProvinces {
  NSArray<NSString*>* options = @[ @"Ontario", @"Quebec" ];
  self.province.value = options[1];
  self.province.enabled = YES;
  [self.consumer setOptions:options forEditorField:self.province];
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
  self.province = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeProfileHomeAddressState
                   fieldType:EditorFieldTypeTextField
                       label:@"Province"
                       value:@"Loading..."
                    required:YES];
  self.province.enabled = NO;
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
  EditorField* save = [[EditorField alloc]
      initWithAutofillUIType:AutofillUITypeCreditCardSaveToChrome
                   fieldType:EditorFieldTypeSwitch
                       label:@"Save"
                       value:@"YES"
                    required:NO];

  return @[ name, country, self.province, address, postalCode, save ];
}

#pragma mark - PaymentRequestEditViewControllerDataSource

- (CollectionViewItem*)headerItem {
  return nil;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

- (void)formatValueForEditorField:(EditorField*)field {
}

- (UIImage*)iconIdentifyingEditorField:(EditorField*)field {
  return nil;
}

#pragma mark - PaymentRequestEditViewControllerValidator

- (NSString*)paymentRequestEditViewController:
                 (PaymentRequestEditViewController*)controller
                                validateField:(EditorField*)field {
  return (!field.value.length && field.isRequired) ? @"Field is required" : nil;
}

@end

@interface SCPaymentsEditorCoordinator ()

@property(nonatomic, strong)
    PaymentRequestEditViewController* paymentRequestEditViewController;

@property(nonatomic, strong) SCPaymentsEditorMediator* mediator;

@property(nonatomic, strong) ProtocolAlerter* alerter;

@end

@implementation SCPaymentsEditorCoordinator

@synthesize baseViewController = _baseViewController;
@synthesize paymentRequestEditViewController =
    _paymentRequestEditViewController;
@synthesize mediator = _mediator;
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
  self.mediator = [[SCPaymentsEditorMediator alloc] init];
  [self.mediator setConsumer:self.paymentRequestEditViewController];
  [self.paymentRequestEditViewController setDataSource:self.mediator];
  [self.paymentRequestEditViewController
      setDelegate:static_cast<id<PaymentRequestEditViewControllerDelegate>>(
                      self.alerter)];
  [self.paymentRequestEditViewController setValidatorDelegate:self.mediator];
  [self.paymentRequestEditViewController loadModel];

  // Set the options for the province field after the model is loaded.
  [self.mediator loadProvinces];

  [self.baseViewController
      pushViewController:self.paymentRequestEditViewController
                animated:YES];
}

@end
