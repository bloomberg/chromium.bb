// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_mediator.h"

#include "components/autofill/core/browser/autofill_profile.h"
#include "ios/chrome/browser/payments/payment_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AddressEditMediator ()

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The address to be edited, if any. This pointer is not owned by this class and
// should outlive it.
@property(nonatomic, assign) autofill::AutofillProfile* address;

// The list of field definitions for the editor.
@property(nonatomic, strong) NSArray<EditorField*>* fields;

@end

@implementation AddressEditMediator

@synthesize state = _state;
@synthesize paymentRequest = _paymentRequest;
@synthesize address = _address;
@synthesize fields = _fields;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
                               address:(autofill::AutofillProfile*)address {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _address = address;
    _fields = [self createEditorFields];
    _state =
        _address ? EditViewControllerStateEdit : EditViewControllerStateCreate;
  }
  return self;
}

- (CollectionViewItem*)headerItem {
  return nil;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

- (NSArray<EditorField*>*)editorFields {
  return self.fields;
}

#pragma mark - Helper methods

- (NSArray<EditorField*>*)createEditorFields {
  return @[];
}

@end
