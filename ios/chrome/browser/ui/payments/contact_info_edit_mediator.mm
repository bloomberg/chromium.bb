// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_edit_mediator.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContactInfoEditMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The profile to be edited, if any. This pointer is not owned by this class and
// should outlive it.
@property(nonatomic, assign) autofill::AutofillProfile* profile;

// The list of current editor fields.
@property(nonatomic, strong) NSMutableArray<EditorField*>* fields;

@end

@implementation ContactInfoEditMediator

@synthesize state = _state;
@synthesize consumer = _consumer;
@synthesize paymentRequest = _paymentRequest;
@synthesize profile = _profile;
@synthesize fields = _fields;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
                               profile:(autofill::AutofillProfile*)profile {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _profile = profile;
    _state =
        _profile ? EditViewControllerStateEdit : EditViewControllerStateCreate;
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setEditorFields:[self createEditorFields]];
}

#pragma mark - PaymentRequestEditViewControllerDataSource

- (CollectionViewItem*)headerItem {
  return nil;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

- (UIImage*)iconIdentifyingEditorField:(EditorField*)field {
  return nil;
}

#pragma mark - Helper methods

// Creates and returns an array of editor fields.
- (NSArray<EditorField*>*)createEditorFields {
  self.fields = [[NSMutableArray alloc] init];

  if (_paymentRequest->request_payer_name()) {
    NSString* name = self.profile
                         ? base::SysUTF16ToNSString(self.profile->GetInfo(
                               autofill::AutofillType(autofill::NAME_FULL),
                               GetApplicationContext()->GetApplicationLocale()))
                         : nil;
    EditorField* nameField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileFullName
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_NAME_FIELD_IN_CONTACT_DETAILS)
                         value:name
                      required:YES];
    [self.fields addObject:nameField];
  }

  if (_paymentRequest->request_payer_phone()) {
    NSString* phone =
        self.profile
            ? base::SysUTF16ToNSString(self.profile->GetInfo(
                  autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
                  GetApplicationContext()->GetApplicationLocale()))
            : nil;
    EditorField* phoneField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_PHONE_FIELD_IN_CONTACT_DETAILS)
                         value:phone
                      required:YES];
    [self.fields addObject:phoneField];
  }

  if (_paymentRequest->request_payer_email()) {
    NSString* email =
        self.profile ? base::SysUTF16ToNSString(self.profile->GetInfo(
                           autofill::AutofillType(autofill::EMAIL_ADDRESS),
                           GetApplicationContext()->GetApplicationLocale()))
                     : nil;
    EditorField* emailField = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileEmailAddress
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(
                                   IDS_PAYMENTS_EMAIL_FIELD_IN_CONTACT_DETAILS)
                         value:email
                      required:YES];
    [self.fields addObject:emailField];
  }

  DCHECK(self.fields.count > 0) << "The contact info editor shouldn't be "
                                   "reachable if no contact information is "
                                   "requested.";
  return self.fields;
}

@end
