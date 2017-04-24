// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation EditorField

@synthesize autofillUIType = _autofillUIType;
@synthesize label = _label;
@synthesize value = _value;
@synthesize required = _required;
@synthesize item = _item;
@synthesize sectionIdentifier = _sectionIdentifier;

- (instancetype)initWithAutofillUIType:(AutofillUIType)autofillUIType
                                 label:(NSString*)label
                                 value:(NSString*)value
                              required:(BOOL)required {
  self = [super init];
  if (self) {
    _autofillUIType = autofillUIType;
    _label = label;
    _value = value;
    _required = required;
  }
  return self;
}

@end
