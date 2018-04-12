// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"

#include <objc/runtime.h>
#include <map>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "ios/web_view/internal/app/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
std::map<SEL, autofill::ServerFieldType> kGettersToType = {
    {@selector(name), autofill::NAME_FULL},
    {@selector(company), autofill::COMPANY_NAME},
    {@selector(address1), autofill::ADDRESS_HOME_LINE1},
    {@selector(address2), autofill::ADDRESS_HOME_LINE2},
    {@selector(city), autofill::ADDRESS_HOME_CITY},
    {@selector(state), autofill::ADDRESS_HOME_STATE},
    {@selector(zipcode), autofill::ADDRESS_HOME_ZIP},
    {@selector(country), autofill::ADDRESS_HOME_COUNTRY},
    {@selector(phone), autofill::PHONE_HOME_WHOLE_NUMBER},
    {@selector(email), autofill::EMAIL_ADDRESS},
};
std::map<SEL, autofill::ServerFieldType> kSettersToType = {
    {@selector(setName:), autofill::NAME_FULL},
    {@selector(setCompany:), autofill::COMPANY_NAME},
    {@selector(setAddress1:), autofill::ADDRESS_HOME_LINE1},
    {@selector(setAddress2:), autofill::ADDRESS_HOME_LINE2},
    {@selector(setCity:), autofill::ADDRESS_HOME_CITY},
    {@selector(setState:), autofill::ADDRESS_HOME_STATE},
    {@selector(setZipcode:), autofill::ADDRESS_HOME_ZIP},
    {@selector(setCountry:), autofill::ADDRESS_HOME_COUNTRY},
    {@selector(setPhone:), autofill::PHONE_HOME_WHOLE_NUMBER},
    {@selector(setEmail:), autofill::EMAIL_ADDRESS},
};
}  // namespace

@implementation CWVAutofillProfile {
  autofill::AutofillProfile _internalProfile;
}

@dynamic name, company, address1, address2, city, state, zipcode, country,
    phone, email;

+ (void)initialize {
  if (self != [CWVAutofillProfile class]) {
    return;
  }

  // Add all getters and setters dynamically.
  for (const auto& pair : kGettersToType) {
    class_addMethod([self class], pair.first, (IMP)propertyGetter, "@@:");
  }
  for (const auto& pair : kSettersToType) {
    class_addMethod([self class], pair.first, (IMP)propertySetter, "v@:@");
  }
}

NSString* propertyGetter(CWVAutofillProfile* self, SEL _cmd) {
  DCHECK(kGettersToType.find(_cmd) != kGettersToType.end());

  autofill::ServerFieldType type = kGettersToType[_cmd];
  const std::string& locale =
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale();
  const base::string16& value =
      self->_internalProfile.GetInfo(autofill::AutofillType(type), locale);
  return base::SysUTF16ToNSString(value);
}

void propertySetter(CWVAutofillProfile* self, SEL _cmd, NSString* value) {
  DCHECK(kSettersToType.find(_cmd) != kSettersToType.end());

  autofill::ServerFieldType type = kSettersToType[_cmd];
  const std::string& locale =
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale();
  self->_internalProfile.SetInfo(type, base::SysNSStringToUTF16(value), locale);
}

- (instancetype)initWithProfile:(const autofill::AutofillProfile&)profile {
  self = [super init];
  if (self) {
    _internalProfile = profile;
  }
  return self;
}

#pragma mark - Internal Methods

- (autofill::AutofillProfile*)internalProfile {
  return &_internalProfile;
}

@end
