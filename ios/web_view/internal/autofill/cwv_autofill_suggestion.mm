// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVAutofillSuggestion

@synthesize formSuggestion = _formSuggestion;
@synthesize formName = _formName;
@synthesize fieldName = _fieldName;
@synthesize fieldIdentifier = _fieldIdentifier;

- (instancetype)initWithFormSuggestion:(FormSuggestion*)formSuggestion
                              formName:(NSString*)formName
                             fieldName:(NSString*)fieldName
                       fieldIdentifier:(NSString*)fieldIdentifier {
  self = [super init];
  if (self) {
    _formSuggestion = formSuggestion;
    _formName = [formName copy];
    _fieldName = [fieldName copy];
    _fieldIdentifier = [fieldIdentifier copy];
  }
  return self;
}

#pragma mark - Public Methods

- (NSString*)value {
  return [_formSuggestion.value copy];
}

- (NSString*)displayDescription {
  return [_formSuggestion.displayDescription copy];
}

- (UIImage* __nullable)icon {
  if ([_formSuggestion.icon length] == 0) {
    return nil;
  }
  int resourceID = autofill::CreditCard::IconResourceId(
      base::SysNSStringToUTF8(_formSuggestion.icon));
  return ui::ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(resourceID)
      .ToUIImage();
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  return [NSString stringWithFormat:@"%@ value: %@, displayDescription: %@",
                                    super.debugDescription, self.value,
                                    self.displayDescription];
}

@end
