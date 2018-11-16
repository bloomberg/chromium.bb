// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_form_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVAutofillForm

- (instancetype)initWithFormStructure:
    (const autofill::FormStructure&)formStructure {
  self = [super init];
  if (self) {
    _name = base::SysUTF16ToNSString(formStructure.form_name());

    _type = CWVAutofillFormTypeUnknown;
    std::set<autofill::FormType> formTypes = formStructure.GetFormTypes();
    if (formTypes.find(autofill::ADDRESS_FORM) != formTypes.end()) {
      _type |= CWVAutofillFormTypeAddresses;
    }
    if (formTypes.find(autofill::CREDIT_CARD_FORM) != formTypes.end()) {
      _type |= CWVAutofillFormTypeCreditCards;
    }
    if (formTypes.find(autofill::PASSWORD_FORM) != formTypes.end()) {
      _type |= CWVAutofillFormTypePasswords;
    }
  }
  return self;
}

@end
